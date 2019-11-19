// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative/rules_registry_service.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/test/base/testing_profile.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/api/declarative/test_rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kExtensionId[] = "foo";

void InsertRule(scoped_refptr<extensions::RulesRegistry> registry,
                const std::string& id) {
  std::vector<extensions::api::events::Rule> add_rules;
  add_rules.emplace_back();
  add_rules[0].id.reset(new std::string(id));
  std::string error = registry->AddRules(kExtensionId, std::move(add_rules));
  EXPECT_TRUE(error.empty());
}

void VerifyNumberOfRules(scoped_refptr<extensions::RulesRegistry> registry,
                         size_t expected_number_of_rules) {
  std::vector<const extensions::api::events::Rule*> get_rules;
  registry->GetAllRules(kExtensionId, &get_rules);
  EXPECT_EQ(expected_number_of_rules, get_rules.size());
}

}  // namespace

namespace extensions {

class RulesRegistryServiceTest : public testing::Test {
 public:
  RulesRegistryServiceTest() = default;

  ~RulesRegistryServiceTest() override {}

  void TearDown() override {
    // Make sure that deletion traits of all registries are executed.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(RulesRegistryServiceTest, TestConstructionAndMultiThreading) {
  RulesRegistryService registry_service(NULL);

  int key = RulesRegistryService::kDefaultRulesRegistryID;
  TestRulesRegistry* ui_registry =
      new TestRulesRegistry(content::BrowserThread::UI, "ui", key);

  TestRulesRegistry* io_registry =
      new TestRulesRegistry(content::BrowserThread::IO, "io", key);

  // Test registration.

  registry_service.RegisterRulesRegistry(base::WrapRefCounted(ui_registry));
  registry_service.RegisterRulesRegistry(base::WrapRefCounted(io_registry));

  EXPECT_TRUE(registry_service.GetRulesRegistry(key, "ui").get());
  EXPECT_TRUE(registry_service.GetRulesRegistry(key, "io").get());
  EXPECT_FALSE(registry_service.GetRulesRegistry(key, "foo").get());

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&InsertRule, registry_service.GetRulesRegistry(key, "ui"),
                     "ui_task"));

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&InsertRule, registry_service.GetRulesRegistry(key, "io"),
                     "io_task"));

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&VerifyNumberOfRules,
                     registry_service.GetRulesRegistry(key, "ui"), 1));

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&VerifyNumberOfRules,
                     registry_service.GetRulesRegistry(key, "io"), 1));

  base::RunLoop().RunUntilIdle();

  // Test extension uninstalling.
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Extension")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Build();
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetID(kExtensionId)
          .Build();
  registry_service.SimulateExtensionUninstalled(extension.get());

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&VerifyNumberOfRules,
                     registry_service.GetRulesRegistry(key, "ui"), 0));

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&VerifyNumberOfRules,
                     registry_service.GetRulesRegistry(key, "io"), 0));

  base::RunLoop().RunUntilIdle();
}

TEST_F(RulesRegistryServiceTest, DefaultRulesRegistryRegistered) {
  struct {
    version_info::Channel channel;
    bool expect_api_enabled;
  } test_cases[] = {
      {version_info::Channel::UNKNOWN, true},
      {version_info::Channel::STABLE, false},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        "Testing Channel %s",
        version_info::GetChannelString(test_case.channel).c_str()));
    ScopedCurrentChannel scoped_channel(test_case.channel);

    ASSERT_EQ(test_case.expect_api_enabled,
              FeatureProvider::GetAPIFeature("declarativeWebRequest")
                  ->IsAvailableToEnvironment()
                  .is_available());

    TestingProfile profile;
    RulesRegistryService registry_service(&profile);

    // The default web request rules registry should only be created if the API
    // is enabled.
    EXPECT_EQ(
        test_case.expect_api_enabled,
        registry_service
                .GetRulesRegistry(RulesRegistryService::kDefaultRulesRegistryID,
                                  declarative_webrequest_constants::kOnRequest)
                .get() != nullptr);

    // Content rules registry should always be created.
    EXPECT_TRUE(registry_service.GetRulesRegistry(
        RulesRegistryService::kDefaultRulesRegistryID,
        declarative_content_constants::kOnPageChanged));
    EXPECT_TRUE(registry_service.content_rules_registry());

    // Rules registries for web views should always be created.
    const int kWebViewRulesRegistryID = 1;
    EXPECT_TRUE(registry_service.GetRulesRegistry(
        kWebViewRulesRegistryID, declarative_webrequest_constants::kOnRequest));
  }
}

}  // namespace extensions
