// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"

#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/policy_types.h"
#include "components/value_store/test_value_store_factory.h"
#include "components/value_store/value_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

using value_store::ValueStore;

constexpr auto kAnyPolicyScope = policy::PolicyScope::POLICY_SCOPE_USER;
constexpr auto kAnyPolicySource = policy::PolicySource::POLICY_SOURCE_PLATFORM;
constexpr auto kAnotherPolicyDomain =
    policy::PolicyDomain::POLICY_DOMAIN_SIGNIN_EXTENSIONS;

base::Value::Dict CreateDict(const std::string& json) {
  auto dict = base::JSONReader::Read(json);
  EXPECT_NE(dict, std::nullopt) << "Invalid json: '" << json << "'";
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

  PolicyBuilder& AddPolicyWithoutProperties(
      scoped_refptr<const Extension> extension) {
    // Calling 'Get' will create an empty policy map for the extension.
    bundle_.Get(
        {policy::PolicyDomain::POLICY_DOMAIN_EXTENSIONS, extension->id()});
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

  void OnSettingsChanged(
      const ExtensionId& extension_id,
      StorageAreaNamespace storage_area,
      std::optional<api::storage::AccessLevel> session_access_level,
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
        /*is_initialization_complete_return=*/false,
        /*is_first_policy_load_complete_return=*/false);

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

  ManagedValueStoreCache& InitializeCache() {
    DCHECK_EQ(cache_, nullptr) << "Can't create cache twice";
    cache_ = std::make_unique<ManagedValueStoreCache>(
        *profile_, factory_, observer_.GetObserverCallback());
    return *cache_;
  }

  void SetPolicyServiceInitialized(bool is_initialized) {
    policy_provider().SetDefaultReturns(
        /*is_initialization_complete_return=*/is_initialized,
        /*is_first_policy_load_complete_return=*/is_initialized);
    // We must trigger a refresh, otherwise the policy service will not
    // read the new values of `IsInitializationComplete()` and
    // `IsFirstPolicyLoadComplete()`.
    policy_provider().RefreshWithSamePolicies();
  }

  void InitializeCacheAndPolicyService() {
    SetPolicyServiceInitialized(true);
    InitializeCache();
  }

  scoped_refptr<const Extension> CreateExtension(const ExtensionId& id) {
    return ExtensionBuilder(id).Build();
  }

  // Sends the new policy values to the policy provider, and wait until the
  // policy has been applied.
  void UpdatePolicyAndWait(PolicyBuilder& new_policy_builder) {
    UpdatePolicyAndWait(new_policy_builder.Build());
  }

  policy::PolicyDomain policy_domain() const {
    return ManagedValueStoreCache::GetPolicyDomain(profile());
  }

  PolicyBuilder GetPolicyBuilder() { return PolicyBuilder(policy_domain()); }

  // Sends the new policy values to the policy provider, and wait until the
  // policy has been applied.
  void UpdatePolicyAndWait(policy::PolicyBundle new_policy) {
    policy_provider().UpdatePolicy(std::move(new_policy));
    content::RunAllTasksUntilIdle();
  }

  ValueStore& GetValueStoreForExtension(
      scoped_refptr<const Extension> extension) {
    base::test::TestFuture<ValueStore*> waiter;

    RunWithValueStoreForExtension(waiter.GetCallback(), extension);

    EXPECT_TRUE(waiter.Wait())
        << "Timeout waiting for value store for extension " << extension->id();
    EXPECT_NE(waiter.Get(), nullptr)
        << "Got nullptr as value store for extension " << extension->id();
    return *waiter.Get();
  }

  // Since RunWithValueStoreForExtension can only be invoked from the backend
  // sequence, this method does a few thread jumps:
  //   1) Invoke RunWithValueStoreForExtension on the Backend sequence
  //   2) This will invoke the base post task callback (on the backend
  //      sequence).
  //   3) This callback will post a task to the current sequence.
  //   4) That task will invoke the TestFuture's callback on the current
  //   sequence.
  void RunWithValueStoreForExtension(
      ManagedValueStoreCache::StorageCallback callback,
      scoped_refptr<const Extension> extension) {
    GetBackendTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ManagedValueStoreCache::RunWithValueStoreForExtension,
                       base::Unretained(&cache()),
                       base::BindPostTaskToCurrentDefault(std::move(callback)),
                       extension));
  }

  TestingProfile& profile() { return *profile_; }
  const TestingProfile& profile() const { return *profile_; }

  ManagedValueStoreCache& cache() {
    DCHECK(cache_) << "Must call InitializeCache() first";
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
  InitializeCacheAndPolicyService();
  auto extension = CreateExtension("ExtensionId-1");

  policy_provider().UpdatePolicy(
      GetPolicyBuilder()
          .AddMandatoryPolicy(extension, "color", "blue")
          .Build());

  ExtensionId extension_id = observer().WaitForPolicyUpdate();
  EXPECT_EQ(extension_id, extension->id());
}

TEST_F(ManagedValueStoreCacheTest,
       ShouldStoreMandatoryPolicyValuesForAnExtension) {
  InitializeCacheAndPolicyService();
  auto extension = CreateExtension("ExtensionId-1");

  UpdatePolicyAndWait(
      GetPolicyBuilder().AddMandatoryPolicy(extension, "color", "red"));

  ValueStore& value_store = GetValueStoreForExtension(extension);
  EXPECT_EQ(value_store.Get("color").settings(),
            CreateDict(R"( { "color": "red" } )"));
}

TEST_F(ManagedValueStoreCacheTest,
       ShouldIgnoreRecommendedPolicyValuesForAnExtension) {
  InitializeCacheAndPolicyService();
  auto extension = CreateExtension("ExtensionId-1");

  UpdatePolicyAndWait(
      GetPolicyBuilder()
          .AddMandatoryPolicy(extension, "mandatory", "<value>")
          .AddRecommendedPolicy(extension, "recommended", "<value-2>")
          .Build());

  ValueStore& value_store = GetValueStoreForExtension(extension);
  EXPECT_EQ(value_store.Get("recommended").settings(), CreateDict(R"( { } )"));
}

TEST_F(ManagedValueStoreCacheTest, ShouldIgnorePoliciesInAnotherDomain) {
  InitializeCacheAndPolicyService();
  auto extension = CreateExtension("ExtensionId-1");

  UpdatePolicyAndWait(GetPolicyBuilder()
                          .AddPolicyInDomain(extension, policy_domain(),
                                             "property", "right-domain")
                          .AddPolicyInDomain(extension, kAnotherPolicyDomain,
                                             "property", "wrong-domain")
                          .Build());

  ValueStore& value_store = GetValueStoreForExtension(extension);
  EXPECT_EQ(value_store.Get("property").settings(),
            CreateDict(R"( { "property" : "right-domain" } )"));
}

TEST_F(ManagedValueStoreCacheTest,
       ValueStoreShouldNotContainValuesOfOtherExtensions) {
  InitializeCacheAndPolicyService();
  auto extension1 = CreateExtension("extension-1");
  auto extension2 = CreateExtension("extension-2");

  UpdatePolicyAndWait(
      GetPolicyBuilder()
          .AddMandatoryPolicy(extension1, "own-property", "value-1")
          .AddMandatoryPolicy(extension2, "other-extension-property", "value-2")
          .Build());

  ValueStore& value_store_1 = GetValueStoreForExtension(extension1);
  EXPECT_EQ(value_store_1.Get("other-extension-property").settings(),
            CreateDict(" {} "));
}

TEST_F(ManagedValueStoreCacheTest, FetchingUnknownValueShouldNotReturnAnError) {
  InitializeCacheAndPolicyService();
  auto extension1 = CreateExtension("extension-1");

  UpdatePolicyAndWait(GetPolicyBuilder()
                          .AddMandatoryPolicy(extension1, "color", "blue")
                          .Build());

  ValueStore& value_store = GetValueStoreForExtension(extension1);

  EXPECT_EQ(value_store.Get("unknown-property").status().code, ValueStore::OK);
  EXPECT_EQ(value_store.Get("unknown-property").settings(), CreateDict(" {} "));
}

TEST_F(ManagedValueStoreCacheTest, FetchingUnsetValueShouldNotReturnAnError) {
  InitializeCacheAndPolicyService();
  auto extension1 = CreateExtension("extension-1");

  UpdatePolicyAndWait(GetPolicyBuilder().AddMandatoryPolicy(
      extension1, "set-property", "value"));

  ValueStore& value_store = GetValueStoreForExtension(extension1);

  EXPECT_EQ(value_store.Get("unset-property").status().code, ValueStore::OK);
  EXPECT_EQ(value_store.Get("unset-property").settings(), CreateDict(" {} "));
}

TEST_F(ManagedValueStoreCacheTest, ShouldBeAbleToFetchAnEmptyValueStore) {
  InitializeCacheAndPolicyService();
  auto extension1 = CreateExtension("extension-1");

  UpdatePolicyAndWait(
      GetPolicyBuilder().AddPolicyWithoutProperties(extension1));

  base::test::TestFuture<ValueStore*> value_store_waiter;
  RunWithValueStoreForExtension(value_store_waiter.GetCallback(), extension1);

  EXPECT_TRUE(value_store_waiter.Wait())
      << "Failed to fetch an empty value store";
}

TEST_F(ManagedValueStoreCacheTest,
       RunWithValueStoreForExtensionShouldWaitUntilPolicyServiceIsInitialized) {
  auto extension1 = CreateExtension("extension-1");

  SetPolicyServiceInitialized(false);
  InitializeCache();

  base::test::TestFuture<ValueStore*> value_store_waiter;
  RunWithValueStoreForExtension(value_store_waiter.GetCallback(), extension1);

  // Value store should not be passed yet.
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(value_store_waiter.IsReady());

  // Even a policy update should be ignored if the policy service is not
  // initialized.
  UpdatePolicyAndWait(GetPolicyBuilder().AddMandatoryPolicy(
      extension1, "set-property", "first-value"));
  EXPECT_FALSE(value_store_waiter.IsReady());

  // But after initializing the policy service RunWithValueStoreForExtension
  // should pass the value store to our waiter.
  SetPolicyServiceInitialized(true);
  EXPECT_TRUE(value_store_waiter.Wait());
}

TEST_F(ManagedValueStoreCacheTest,
       ShouldStorePolicyValuesSetBeforePolicyServiceIsInitialized) {
  auto extension = CreateExtension("extension");

  SetPolicyServiceInitialized(false);
  InitializeCache();

  // Set a policy before the policy service is initialized.
  UpdatePolicyAndWait(GetPolicyBuilder().AddMandatoryPolicy(
      extension, "property", "early-value"));

  SetPolicyServiceInitialized(true);

  ValueStore& value_store = GetValueStoreForExtension(extension);

  EXPECT_EQ(value_store.Get("property").settings(),
            CreateDict(R"( { "property": "early-value" } )"));
}

TEST_F(ManagedValueStoreCacheTest,
       RunWithValueStoreForExtensionCallbackShouldOnlyBeInvokedOnce) {
  auto extension1 = CreateExtension("extension-1");

  SetPolicyServiceInitialized(false);
  InitializeCache();

  base::test::RepeatingTestFuture<ValueStore*> value_store_waiter;
  RunWithValueStoreForExtension(value_store_waiter.GetCallback(), extension1);
  content::RunAllTasksUntilIdle();

  // Value store should not be passed yet, as the policy service is not
  // initialized.
  ASSERT_TRUE(value_store_waiter.IsEmpty());

  // After initializing the policy service the callback should be invoked, and
  // the value store waiter should be ready.
  SetPolicyServiceInitialized(true);
  UpdatePolicyAndWait(GetPolicyBuilder().AddMandatoryPolicy(
      extension1, "set-property", "first-value"));
  value_store_waiter.Take();

  // Consecutive policy updates should not call the callback again.
  UpdatePolicyAndWait(GetPolicyBuilder().AddMandatoryPolicy(
      extension1, "set-property", "other-value"));
  EXPECT_TRUE(value_store_waiter.IsEmpty());
}

}  // namespace
}  // namespace extensions
