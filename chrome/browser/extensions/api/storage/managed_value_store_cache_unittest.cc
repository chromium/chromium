// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/value_store/test_value_store_factory.h"
#include "components/value_store/value_store.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

using value_store::ValueStore;

constexpr auto kAnyPolicyScope = policy::PolicyScope::POLICY_SCOPE_USER;
constexpr auto kAnyPolicySource = policy::PolicySource::POLICY_SOURCE_PLATFORM;
constexpr auto kAnotherPolicyDomain =
    policy::PolicyDomain::POLICY_DOMAIN_SIGNIN_EXTENSIONS;

policy::Schema CreateSchema(const std::string& value) {
  std::string error;
  policy::Schema result = policy::Schema::Parse(value, &error);
  EXPECT_EQ(error, "") << "Error parsing schema '" << value << "'";
  return result;
}

policy::Schema SchemaWithoutProperties() {
  return CreateSchema(R"(
      {
        "type": "object",
        "properties": {
        }
      }
    )");
}

// A schema that has a single property with the given name.
policy::Schema SchemaWithProperty(const std::string& property) {
  return CreateSchema(base::StringPrintf(
      R"(
      {
        "type": "object",
        "properties": {
            "%s" : { "type": "string"},
        }
      }
    )",
      property.c_str()));
}

// A schema that has two properties with the given names.
policy::Schema SchemaWithProperties(const std::string& property1,
                                    const std::string& property2) {
  return CreateSchema(base::StringPrintf(
      R"(
      {
        "type": "object",
        "properties": {
            "%s" : { "type": "string"},
            "%s" : { "type": "string"},
        }
      }
    )",
      property1.c_str(), property2.c_str()));
}

base::Value::Dict CreateDict(const std::string& json) {
  auto dict = base::JSONReader::Read(json);
  EXPECT_NE(dict, absl::nullopt) << "Invalid json: '" << json << "'";
  return std::move(dict.value()).TakeDict();
}

class PolicyBuilder {
 public:
  explicit PolicyBuilder(policy::PolicyDomain default_domain)
      : domain_(default_domain) {}
  ~PolicyBuilder() = default;

  PolicyBuilder& AddMandatoryPolicy(scoped_refptr<const Extension> extension,
                                    const std::string& key,
                                    const std::string& value) {
    bundle_.Get({domain_, extension->id()})
        .Set(key, policy::POLICY_LEVEL_MANDATORY, kAnyPolicyScope,
             kAnyPolicySource, base::Value(value), nullptr);
    return *this;
  }

  PolicyBuilder& AddRecommendedPolicy(scoped_refptr<const Extension> extension,
                                      const std::string& key,
                                      const std::string& value) {
    bundle_.Get({domain_, extension->id()})
        .Set(key, policy::POLICY_LEVEL_RECOMMENDED, kAnyPolicyScope,
             kAnyPolicySource, base::Value(value), nullptr);
    return *this;
  }

  PolicyBuilder& AddPolicyInDomain(scoped_refptr<const Extension> extension,
                                   policy::PolicyDomain domain,
                                   const std::string& key,
                                   const std::string& value) {
    bundle_.Get({domain, extension->id()})
        .Set(key, policy::POLICY_LEVEL_MANDATORY, kAnyPolicyScope,
             kAnyPolicySource, base::Value(value), nullptr);
    return *this;
  }

  policy::PolicyBundle Build() { return std::move(bundle_); }

 private:
  // The domain that will be used for any policy added (unless explicitly
  // specified otherwise)
  const policy::PolicyDomain domain_;
  policy::PolicyBundle bundle_;
};

class FakeSettingsObserver {
 public:
  FakeSettingsObserver() = default;
  FakeSettingsObserver(const FakeSettingsObserver&) = delete;
  FakeSettingsObserver& operator=(const FakeSettingsObserver&) = delete;
  ~FakeSettingsObserver() = default;

  void OnSettingsChanged(const std::string& extension_id,
                         StorageAreaNamespace storage_area,
                         base::Value changes) {
    future_.AddValue(extension_id);
  }

  std::string WaitForPolicyUpdate() {
    EXPECT_TRUE(future_.Wait())
        << "Settings-changed-callback was never invoked";
    return future_.Take();
  }

  SettingsChangedCallback GetObserverCallback() {
    return base::BindRepeating(&FakeSettingsObserver::OnSettingsChanged,
                               base::Unretained(this));
  }

 private:
  base::test::RepeatingTestFuture<std::string> future_;
};

class ManagedValueStoreCacheTest : public testing::Test {
 public:
  ManagedValueStoreCacheTest() = default;
  ~ManagedValueStoreCacheTest() override = default;

  // testing::Test implementation:
  void SetUp() override {
    policy_provider_ = std::make_unique<
        testing::NiceMock<policy::MockConfigurationPolicyProvider>>();
    policy_provider_->SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);

    policy::PolicyServiceImpl::Providers providers{policy_provider_.get()};
    auto policy_service =
        std::make_unique<policy::PolicyServiceImpl>(providers);

    profile_ = TestingProfile::Builder()
                   .SetPolicyService(std::move(policy_service))
                   .Build();
  }

  void TearDown() override {
    if (cache_) {
      cache_->ShutdownOnUI();

      // Even though the cache was created on the UI thread, it must be
      // destroyed on the backend thread.
      GetBackendTaskRunner()->DeleteSoon(FROM_HERE, std::move(cache_));
    }
  }

  void CreateCache() {
    DCHECK_EQ(cache_, nullptr);
    cache_ = std::make_unique<ManagedValueStoreCache>(
        profile_.get(), factory_, observer_.GetObserverCallback());
  }

  scoped_refptr<const Extension> CreateExtension(const std::string& id) {
    return ExtensionBuilder(id).Build();
  }

  // Informs the Schema registry that the schema of this extension has been
  // loaded from the disk.
  void SetExtensionSchema(const Extension& extension,
                          const policy::Schema& schema) {
    policy::SchemaRegistry* registry =
        profile().GetPolicySchemaRegistryService()->registry();
    registry->RegisterComponent(
        {policy::PolicyDomain::POLICY_DOMAIN_EXTENSIONS, extension.id()},
        schema);
  }

  // Creates an extension with the given id, and register the given schema
  // with this extension. This simulates that the schema has been loaded from
  // the disk.
  scoped_refptr<const Extension> CreateExtensionWithSchema(
      const std::string& extension_id,
      const policy::Schema& schema) {
    auto extension = CreateExtension(extension_id);
    SetExtensionSchema(*extension, schema);
    return extension;
  }

  // Sends the new policy values to the policy provider, and wait until the
  // policy has been applied.
  void UpdatePolicy(PolicyBuilder& new_policy_builder) {
    UpdatePolicy(new_policy_builder.Build());
  }

  policy::PolicyDomain policy_domain() const { return cache_->policy_domain(); }

  PolicyBuilder GetPolicyBuilder() {
    return PolicyBuilder(cache_->policy_domain());
  }

  // Sends the new policy values to the policy provider, and wait until the
  // policy has been applied.
  void UpdatePolicy(policy::PolicyBundle new_policy) {
    EXPECT_NE(cache_, nullptr) << "Call CreateCache() first";
    policy_provider().UpdatePolicy(std::move(new_policy));
    observer().WaitForPolicyUpdate();
  }

  ValueStore& GetValueStoreForExtension(
      scoped_refptr<const Extension> extension) {
    base::test::TestFuture<ValueStore*> waiter;

    // Since RunWithValueStoreForExtension can only be invoked from the backend
    // sequence, we have to do a few thread jumps:
    //   1) Invoke RunWithValueStoreForExtension on the Backend sequence
    //   2) This will invoke the base post task callback (on the backend
    //      sequence).
    //   3) This callback will post a task to the current sequence.
    //   4) That task will invoke the TestFuture's callback on the current
    //   sequence.
    GetBackendTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ManagedValueStoreCache::RunWithValueStoreForExtension,
            base::Unretained(&cache()),
            base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                               waiter.GetCallback()),
            extension));

    EXPECT_TRUE(waiter.Wait())
        << "Timeout waiting for value store for extension " << extension->id();
    EXPECT_NE(waiter.Get(), nullptr)
        << "Got nullptr as value store for extension " << extension->id();
    return *waiter.Get();
  }

  TestingProfile& profile() { return *profile_; }

  ManagedValueStoreCache& cache() {
    EXPECT_NE(cache_, nullptr) << "Call CreateCache() first";
    return *cache_;
  }

  policy::MockConfigurationPolicyProvider& policy_provider() {
    return *policy_provider_;
  }
  FakeSettingsObserver& observer() { return observer_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  FakeSettingsObserver observer_;
  scoped_refptr<value_store::ValueStoreFactory> factory_ =
      base::MakeRefCounted<value_store::TestValueStoreFactory>();
  std::unique_ptr<policy::MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ManagedValueStoreCache> cache_;
};

TEST_F(ManagedValueStoreCacheTest,
       ShouldInformObserverWhenPolicyValuesAreUpdated) {
  CreateCache();
  auto extension =
      CreateExtensionWithSchema("ExtensionId-1", SchemaWithProperty("color"));

  policy_provider().UpdatePolicy(
      GetPolicyBuilder()
          .AddMandatoryPolicy(extension, "color", "blue")
          .Build());

  std::string extension_id = observer().WaitForPolicyUpdate();
  EXPECT_EQ(extension_id, extension->id());
}

TEST_F(ManagedValueStoreCacheTest,
       ShouldStoreMandatoryPolicyValuesForAnExtension) {
  CreateCache();
  auto extension =
      CreateExtensionWithSchema("ExtensionId-1", SchemaWithProperty("color"));

  UpdatePolicy(
      GetPolicyBuilder().AddMandatoryPolicy(extension, "color", "red"));

  ValueStore& value_store = GetValueStoreForExtension(extension);
  EXPECT_EQ(value_store.Get("color").settings(),
            CreateDict(R"( { "color": "red" } )"));
}

TEST_F(ManagedValueStoreCacheTest,
       ShouldIgnoreRecommendedPolicyValuesForAnExtension) {
  CreateCache();
  auto extension = CreateExtensionWithSchema(
      "ExtensionId-1", SchemaWithProperties("mandatory", "recommended"));

  UpdatePolicy(GetPolicyBuilder()
                   .AddMandatoryPolicy(extension, "mandatory", "<value>")
                   .AddRecommendedPolicy(extension, "recommended", "<value-2>")
                   .Build());

  ValueStore& value_store = GetValueStoreForExtension(extension);
  EXPECT_EQ(value_store.Get("recommended").settings(), CreateDict(R"( { } )"));
}

TEST_F(ManagedValueStoreCacheTest, ShouldIgnorePoliciesInAnotherDomain) {
  CreateCache();
  auto extension = CreateExtensionWithSchema("ExtensionId-1",
                                             SchemaWithProperty("property"));

  UpdatePolicy(GetPolicyBuilder()
                   .AddPolicyInDomain(extension, policy_domain(), "property",
                                      "right-domain")
                   .AddPolicyInDomain(extension, kAnotherPolicyDomain,
                                      "property", "wrong-domain")
                   .Build());

  ValueStore& value_store = GetValueStoreForExtension(extension);
  EXPECT_EQ(value_store.Get("property").settings(),
            CreateDict(R"( { "property" : "right-domain" } )"));
}

TEST_F(ManagedValueStoreCacheTest,
       ValueStoreShouldNotContainValuesOfOtherExtensions) {
  CreateCache();
  auto extension1 = CreateExtensionWithSchema(
      "extension-1", SchemaWithProperty("own-property"));
  auto extension2 = CreateExtensionWithSchema(
      "extension-2", SchemaWithProperty("other-extension-property"));

  UpdatePolicy(
      GetPolicyBuilder()
          .AddMandatoryPolicy(extension2, "other-extension-property", "value-2")
          .Build());

  ValueStore& value_store_1 = GetValueStoreForExtension(extension1);
  EXPECT_EQ(value_store_1.Get("other-extension-property").settings(),
            CreateDict(" {} "));
}

TEST_F(ManagedValueStoreCacheTest, FetchingUnknownValueShouldNotReturnAnError) {
  CreateCache();
  auto extension1 =
      CreateExtensionWithSchema("extension-1", SchemaWithoutProperties());

  ValueStore& value_store = GetValueStoreForExtension(extension1);

  EXPECT_EQ(value_store.Get("unknown-property").status().code, ValueStore::OK);
  EXPECT_EQ(value_store.Get("unknown-property").settings(), CreateDict(" {} "));
}

TEST_F(ManagedValueStoreCacheTest, FetchingUnsetValueShouldNotReturnAnError) {
  CreateCache();
  auto extension1 = CreateExtensionWithSchema(
      "extension-1", SchemaWithProperties("set-property", "unset-property"));

  UpdatePolicy(GetPolicyBuilder().AddMandatoryPolicy(extension1, "set-property",
                                                     "value"));

  ValueStore& value_store = GetValueStoreForExtension(extension1);

  EXPECT_EQ(value_store.Get("unset-property").status().code, ValueStore::OK);
  EXPECT_EQ(value_store.Get("unset-property").settings(), CreateDict(" {} "));
}

}  // namespace
}  // namespace extensions
