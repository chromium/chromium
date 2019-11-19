// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/subresource_filter_component_installer.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

static const char kTestRulesetVersion[] = "1.2.3.4";

class TestRulesetService : public subresource_filter::RulesetService {
 public:
  TestRulesetService(
      PrefService* local_state,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::FilePath& base_dir,
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
      : subresource_filter::RulesetService(local_state,
                                           task_runner,
                                           base_dir,
                                           blocking_task_runner) {}

  ~TestRulesetService() override {}

  using UnindexedRulesetInfo = subresource_filter::UnindexedRulesetInfo;
  void IndexAndStoreAndPublishRulesetIfNeeded(
      const UnindexedRulesetInfo& unindexed_ruleset_info) override {
    unindexed_ruleset_info_ = unindexed_ruleset_info;
  }

  const base::FilePath& ruleset_path() const {
    return unindexed_ruleset_info_.ruleset_path;
  }

  const base::FilePath& license_path() const {
    return unindexed_ruleset_info_.license_path;
  }

  const std::string& content_version() const {
    return unindexed_ruleset_info_.content_version;
  }

 private:
  UnindexedRulesetInfo unindexed_ruleset_info_;

  DISALLOW_COPY_AND_ASSIGN(TestRulesetService);
};

class SubresourceFilterMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  SubresourceFilterMockComponentUpdateService() {}
  ~SubresourceFilterMockComponentUpdateService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterMockComponentUpdateService);
};

subresource_filter::Configuration CreateConfigUsingRulesetFlavor(
    const std::string& ruleset_flavor) {
  subresource_filter::Configuration config;
  config.general_settings.ruleset_flavor = ruleset_flavor;
  return config;
}

}  //  namespace

namespace component_updater {

class SubresourceFilterComponentInstallerTest : public PlatformTest {
 public:
  SubresourceFilterComponentInstallerTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(ruleset_service_dir_.CreateUniqueTempDir());
    subresource_filter::IndexedRulesetVersion::RegisterPrefs(
        pref_service_.registry());

    auto test_ruleset_service = std::make_unique<TestRulesetService>(
        &pref_service_, base::ThreadTaskRunnerHandle::Get(),
        ruleset_service_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get());
    test_ruleset_service_ = test_ruleset_service.get();

    TestingBrowserProcess::GetGlobal()->SetRulesetService(
        std::move(test_ruleset_service));
    policy_ = std::make_unique<SubresourceFilterComponentInstallerPolicy>();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetRulesetService(nullptr);
    task_environment_.RunUntilIdle();
    PlatformTest::TearDown();
  }

  TestRulesetService* service() { return test_ruleset_service_; }

  void WriteStringToFile(const std::string data, const base::FilePath& path) {
    ASSERT_EQ(static_cast<int32_t>(data.length()),
              base::WriteFile(path, data.data(), data.length()));
  }

  base::FilePath component_install_dir() {
    return component_install_dir_.GetPath();
  }

  // If |license_contents| is null, no license file will be created.
  void CreateTestSubresourceFilterRuleset(const std::string& ruleset_contents,
                                          const std::string* license_contents) {
    base::FilePath ruleset_data_path = component_install_dir().Append(
        subresource_filter::kUnindexedRulesetDataFileName);
    ASSERT_NO_FATAL_FAILURE(
        WriteStringToFile(ruleset_contents, ruleset_data_path));

    base::FilePath license_path = component_install_dir().Append(
        subresource_filter::kUnindexedRulesetLicenseFileName);
    if (license_contents) {
      ASSERT_NO_FATAL_FAILURE(
          WriteStringToFile(*license_contents, license_path));
    }
  }

  void LoadSubresourceFilterRuleset(int ruleset_format) {
    std::unique_ptr<base::DictionaryValue> manifest(new base::DictionaryValue);
    manifest->SetInteger(
        SubresourceFilterComponentInstallerPolicy::kManifestRulesetFormatKey,
        ruleset_format);
    ASSERT_TRUE(
        policy_->VerifyInstallation(*manifest, component_install_dir()));
    const base::Version expected_version(kTestRulesetVersion);
    policy_->ComponentReady(expected_version, component_install_dir(),
                            std::move(manifest));
    base::RunLoop().RunUntilIdle();
  }

  update_client::InstallerAttributes GetInstallerAttributes() {
    return policy_->GetInstallerAttributes();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:

  base::ScopedTempDir component_install_dir_;
  base::ScopedTempDir ruleset_service_dir_;

  std::unique_ptr<SubresourceFilterComponentInstallerPolicy> policy_;
  TestingPrefServiceSimple pref_service_;

  TestRulesetService* test_ruleset_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterComponentInstallerTest);
};

TEST_F(SubresourceFilterComponentInstallerTest,
       TestComponentRegistrationWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(
      subresource_filter::kSafeBrowsingSubresourceFilter);
  std::unique_ptr<SubresourceFilterMockComponentUpdateService>
      component_updater(new SubresourceFilterMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_)).Times(0);
  RegisterSubresourceFilterComponent(component_updater.get());
  task_environment_.RunUntilIdle();
}

TEST_F(SubresourceFilterComponentInstallerTest,
       TestComponentRegistrationWhenFeatureEnabled) {
  base::test::ScopedFeatureList scoped_enable;
  scoped_enable.InitAndEnableFeature(
      subresource_filter::kSafeBrowsingSubresourceFilter);
  std::unique_ptr<SubresourceFilterMockComponentUpdateService>
      component_updater(new SubresourceFilterMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));
  RegisterSubresourceFilterComponent(component_updater.get());
  task_environment_.RunUntilIdle();
}

TEST_F(SubresourceFilterComponentInstallerTest, LoadEmptyRuleset) {
  ASSERT_TRUE(service());
  ASSERT_NO_FATAL_FAILURE(
      CreateTestSubresourceFilterRuleset(std::string(), nullptr));
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRuleset(
      SubresourceFilterComponentInstallerPolicy::kCurrentRulesetFormat));
  EXPECT_EQ(kTestRulesetVersion, service()->content_version());
  std::string actual_ruleset_contents;
  ASSERT_TRUE(base::ReadFileToString(service()->ruleset_path(),
                                     &actual_ruleset_contents));
  EXPECT_TRUE(actual_ruleset_contents.empty()) << actual_ruleset_contents;
  EXPECT_FALSE(base::PathExists(service()->license_path()));
}

TEST_F(SubresourceFilterComponentInstallerTest, FutureVersionIgnored) {
  ASSERT_TRUE(service());
  const std::string expected_ruleset_contents = "future stuff";
  ASSERT_NO_FATAL_FAILURE(
      CreateTestSubresourceFilterRuleset(expected_ruleset_contents, nullptr));
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRuleset(
      SubresourceFilterComponentInstallerPolicy::kCurrentRulesetFormat + 1));
  EXPECT_EQ(std::string(), service()->content_version());
  EXPECT_TRUE(service()->ruleset_path().empty());
  EXPECT_TRUE(service()->license_path().empty());
}

TEST_F(SubresourceFilterComponentInstallerTest, LoadFileWithData) {
  ASSERT_TRUE(service());
  const std::string expected_ruleset_contents = "foobar";
  const std::string expected_license_contents = "license";
  ASSERT_NO_FATAL_FAILURE(CreateTestSubresourceFilterRuleset(
      expected_ruleset_contents, &expected_license_contents));
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRuleset(
      SubresourceFilterComponentInstallerPolicy::kCurrentRulesetFormat));
  EXPECT_EQ(kTestRulesetVersion, service()->content_version());
  std::string actual_ruleset_contents;
  std::string actual_license_contents;
  ASSERT_TRUE(base::ReadFileToString(service()->ruleset_path(),
                                     &actual_ruleset_contents));
  EXPECT_EQ(expected_ruleset_contents, actual_ruleset_contents);
  ASSERT_TRUE(base::ReadFileToString(service()->license_path(),
                                     &actual_license_contents));
  EXPECT_EQ(expected_license_contents, actual_license_contents);
}

TEST_F(SubresourceFilterComponentInstallerTest, InstallerTag) {
  const struct {
    const char* expected_installer_tag_selected;
    std::vector<std::string> ruleset_flavors;
  } kTestCases[] = {{"", std::vector<std::string>()},
                    {"", {""}},
                    {"a", {"a"}},
                    {"b", {"b"}},
                    {"c", {"c"}},
                    {"d", {"d"}},
                    {"invalid", {"e"}},
                    {"invalid", {"foo"}},
                    {"", {"", ""}},
                    {"a", {"a", ""}},
                    {"a", {"", "a"}},
                    {"a", {"a", "a"}},
                    {"c", {"b", "", "c"}},
                    {"b", {"", "b", "a"}},
                    {"c", {"aaa", "c", "aba"}},
                    {"invalid", {"", "a", "e"}},
                    {"invalid", {"foo", "a", "b"}}};

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(::testing::Message()
                 << "ruleset_flavors: "
                 << ::testing::PrintToString(test_case.ruleset_flavors));

    std::vector<subresource_filter::Configuration> configs;
    for (const auto& ruleset_flavor : test_case.ruleset_flavors)
      configs.push_back(CreateConfigUsingRulesetFlavor(ruleset_flavor));
    subresource_filter::testing::ScopedSubresourceFilterConfigurator
        scoped_configuration(std::move(configs));

    EXPECT_EQ(test_case.expected_installer_tag_selected,
              SubresourceFilterComponentInstallerPolicy::GetInstallerTag());
  }
}

TEST_F(SubresourceFilterComponentInstallerTest, InstallerAttributesDefault) {
  subresource_filter::testing::ScopedSubresourceFilterConfigurator
      scoped_configuration((subresource_filter::Configuration()));
  EXPECT_EQ(update_client::InstallerAttributes(), GetInstallerAttributes());
}

TEST_F(SubresourceFilterComponentInstallerTest, InstallerAttributesCustomTag) {
  constexpr char kTagKey[] = "tag";
  constexpr char kTagValue[] = "a";

  subresource_filter::testing::ScopedSubresourceFilterConfigurator
      scoped_configuration(CreateConfigUsingRulesetFlavor(kTagValue));
  EXPECT_EQ(update_client::InstallerAttributes({{kTagKey, kTagValue}}),
            GetInstallerAttributes());
}

}  // namespace component_updater
