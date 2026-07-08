// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_service.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/component_updater/indigo_component_installer.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/indigo/proto/indigo_config.pb.h"
#include "chrome/browser/indigo/proto/indigo_prompts.pb.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#elif BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#endif

namespace indigo {

class IndigoServiceTest : public testing::Test {
 public:
  IndigoServiceTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIndigo,
        {{features::kIndigoAllowForEnterprise.name, "true"}});
  }

  void SetUp() override {
    ::indigo::prefs::RegisterProfilePrefs(prefs_.registry());
    if (set_script_switch_in_setup_) {
      scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
          "indigo-script", "/dummy/path");
    }
  }

  void TearDown() override {
    component_updater::ResetIndigoInstallDirForTesting();
    glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
  }

  void CreateService() {
    service_ = std::make_unique<IndigoService>(
        &profile_, identity_test_env_.identity_manager(), &prefs_);
    service_->SetRemoteEligibilityFetcherForTesting(base::BindRepeating(
        [](IndigoServiceTest* test,
           IndigoService::RemoteEligibilityCallback callback) {
          test->remote_eligibility_fetch_count_++;
          if (test->auto_complete_remote_eligibility_fetch_) {
            std::move(callback).Run(test->mock_remote_eligibility_);
          } else {
            test->pending_remote_eligibility_callback_ = std::move(callback);
          }
        },
        base::Unretained(this)));
  }

  void MakeAccountAvailableAndCapable(
      std::string_view email = "test@non.managed.com",
      std::string_view hosted_domain = "") {
    AccountInfo info = identity_test_env_.MakePrimaryAccountAvailable(
        std::string(email), signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&info);
    mutator.set_can_use_model_execution_features(true);
    if (!hosted_domain.empty()) {
      AccountInfo::Builder builder(info);
      builder.SetHostedDomain(std::string(hosted_domain));
      info = builder.Build();
    }
    identity_test_env_.UpdateAccountInfoForAccount(info);
  }

  void SetPolicySettings(prefs::Policy value) {
    prefs_.SetInteger(prefs::kIndigoPolicy, value);
  }

  CombinedEligibility GetCombinedEligibility() {
    base::test::TestFuture<CombinedEligibility> future;
    service_->GetCombinedEligibility(
        future.GetCallback<const CombinedEligibility&>());
    return future.Get();
  }

  void CompleteRemoteEligibilityFetch(
      base::expected<RemoteEligibility, std::string> status =
          base::ok(RemoteEligibility{.is_service_supported_for_account = true,
                                     .has_user_image = true})) {
    if (pending_remote_eligibility_callback_) {
      std::move(pending_remote_eligibility_callback_).Run(std::move(status));
    }
  }

  ::testing::AssertionResult LocalEligibilityBecomes(
      ::testing::Matcher<LocalEligibility> matcher) {
    if (matcher.Matches(service_->GetLocalEligibility())) {
      return ::testing::AssertionSuccess();
    }
    base::test::TestFuture<LocalEligibility> future{
        base::test::TestFutureMode::kQueue};
    auto sub = service_->RegisterLocalEligibilityChangedCallback(
        future.GetRepeatingCallback());
    while (future.Wait()) {
      LocalEligibility eligibility = future.Take();
      if (eligibility != service_->GetLocalEligibility()) {
        return ::testing::AssertionFailure()
               << "notification doesn't match the current eligibility";
      }
      if (matcher.Matches(eligibility)) {
        return ::testing::AssertionSuccess();
      }
    }
    return ::testing::AssertionFailure() << "timed out";
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  TestingPrefServiceSimple prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<IndigoService> service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  RemoteEligibility mock_remote_eligibility_ =
      RemoteEligibility{.is_service_supported_for_account = true,
                        .has_user_image = true};
  int remote_eligibility_fetch_count_ = 0;
  IndigoService::RemoteEligibilityCallback pending_remote_eligibility_callback_;
  bool auto_complete_remote_eligibility_fetch_ = true;
  bool set_script_switch_in_setup_ = true;
  base::test::ScopedCommandLine scoped_command_line_;
};

TEST_F(IndigoServiceTest, DefaultStateNotSignedIn) {
  CreateService();
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kNotSignedIn);
}

TEST_F(IndigoServiceTest, SignIn) {
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));
}

TEST_F(IndigoServiceTest, CapabilitiesDisable) {
  CreateService();

  AccountInfo info = identity_test_env_.MakePrimaryAccountAvailable(
      "test@non.managed.com", signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&info);
  mutator.set_can_use_model_execution_features(false);
  identity_test_env_.UpdateAccountInfoForAccount(info);

  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kMissingCapabilities));
}

TEST_F(IndigoServiceTest, RefreshTokenError) {
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  identity_test_env_.SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(LocalEligibilityBecomes(
      LocalEligibility::kRefreshTokenInPersistentErrorState));
  EXPECT_TRUE(service_->IsLocallyEligible());
}

TEST_F(IndigoServiceTest, RefreshTokenErrorResolved) {
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  identity_test_env_.SetInvalidRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(LocalEligibilityBecomes(
      LocalEligibility::kRefreshTokenInPersistentErrorState));

  identity_test_env_.SetRefreshTokenForPrimaryAccount();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));
}

TEST_F(IndigoServiceTest, GlicRequirementEnabledAndDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kIndigo, {{features::kIndigoRequireGlicEnabling.name, "true"},
                          {features::kIndigoAllowForEnterprise.name, "true"}});

  CreateService();
  MakeAccountAvailableAndCapable();

  // Initially Glic is not enabled for the profile, so local eligibility becomes
  // kGlicDisabledForProfile.
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(false);
  EXPECT_TRUE(
      LocalEligibilityBecomes(LocalEligibility::kGlicDisabledForProfile));

  // Once Glic is enabled (bypassing enablement checks), local eligibility
  // becomes kEligible.
  glic::GlicEnabling::SetBypassEnablementChecksForTesting(true);
  {
    CoreAccountId account_id =
        identity_test_env_.identity_manager()->GetPrimaryAccountId(
            signin::ConsentLevel::kSignin);
    AccountInfo info = identity_test_env_.identity_manager()
                           ->FindExtendedAccountInfoByAccountId(account_id);
    service_->OnExtendedAccountInfoUpdated(info);
  }
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));
}

TEST_F(IndigoServiceTest, AnchoredMessageTrigger) {
#if BUILDFLAG(IS_MAC)
  // TODO(crbug.com/434660312): Re-enable on macOS 26 once issues with
  // unexpected test timeout failures are resolved.
  if (base::mac::MacOSMajorVersion() == 26) {
    GTEST_SKIP() << "Disabled on macOS Tahoe.";
  }
#endif
  CreateService();

  EXPECT_TRUE(service_->CanShowAnchoredMessage());
  service_->AnchoredMessageShown();
  EXPECT_FALSE(service_->CanShowAnchoredMessage());

  task_environment_.FastForwardBy(
      features::kIndigoAnchoredMessageResetDuration.Get());
  EXPECT_TRUE(service_->CanShowAnchoredMessage());
}

TEST_F(IndigoServiceTest, RemoteEligibilityUnsupported) {
  mock_remote_eligibility_ = RemoteEligibility{
      .is_service_supported_for_account = false, .has_user_image = false};
  CreateService();

  MakeAccountAvailableAndCapable();

  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  CombinedEligibility combined_eligibility = GetCombinedEligibility();
  EXPECT_TRUE(combined_eligibility.remote_eligibility.has_value());
  EXPECT_FALSE(combined_eligibility.remote_eligibility
                   ->is_service_supported_for_account);
  EXPECT_FALSE(combined_eligibility.remote_eligibility->has_user_image);
}

TEST_F(IndigoServiceTest, MultipleCallsConsecutively_TriggersOneFetch) {
  auto_complete_remote_eligibility_fetch_ = false;
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  base::test::TestFuture<CombinedEligibility> future1;
  base::test::TestFuture<CombinedEligibility> future2;

  service_->GetCombinedEligibility(
      future1.GetCallback<const CombinedEligibility&>());
  service_->GetCombinedEligibility(
      future2.GetCallback<const CombinedEligibility&>());

  CompleteRemoteEligibilityFetch();

  EXPECT_TRUE(future1.Get().remote_eligibility.has_value());
  EXPECT_TRUE(future2.Get().remote_eligibility.has_value());
  EXPECT_EQ(remote_eligibility_fetch_count_, 1);
}

TEST_F(IndigoServiceTest, CallsAfterCompletion_TriggersNewFetch) {
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  CombinedEligibility combined_eligibility = GetCombinedEligibility();
  EXPECT_EQ(remote_eligibility_fetch_count_, 1);
  EXPECT_TRUE(combined_eligibility.remote_eligibility.has_value());

  // Next call should trigger a new fetch.
  combined_eligibility = GetCombinedEligibility();
  EXPECT_EQ(remote_eligibility_fetch_count_, 2);
  EXPECT_TRUE(combined_eligibility.remote_eligibility.has_value());
}

TEST_F(IndigoServiceTest, ErrorMessageStored) {
  auto_complete_remote_eligibility_fetch_ = false;
  CreateService();
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));

  base::test::TestFuture<CombinedEligibility> future;
  service_->GetCombinedEligibility(
      future.GetCallback<const CombinedEligibility&>());

  CompleteRemoteEligibilityFetch(base::unexpected("Server down"));

  CombinedEligibility combined_eligibility = future.Get();
  EXPECT_FALSE(combined_eligibility.remote_eligibility.has_value());
  EXPECT_EQ(combined_eligibility.remote_eligibility.error(), "Server down");
}

class IndigoServiceNoScriptTest : public IndigoServiceTest {
 public:
  IndigoServiceNoScriptTest() { set_script_switch_in_setup_ = false; }
};

TEST_F(IndigoServiceNoScriptTest, ScriptNotAvailable) {
  CreateService();
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kMissingScript);
}

TEST_F(IndigoServiceNoScriptTest, DynamicComponentReady) {
  CreateService();
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kMissingScript);

  base::test::TestFuture<LocalEligibility> future;
  auto sub = service_->RegisterLocalEligibilityChangedCallback(
      future.GetRepeatingCallback());

  // Simulate component ready.
  component_updater::IndigoComponentInstallerPolicy policy;
  policy.ComponentReady(base::Version("1.0"),
                        base::FilePath(FILE_PATH_LITERAL("/dummy/path")),
                        base::DictValue());

  // It should transition to kNotSignedIn (since we are not signed in).
  EXPECT_EQ(future.Take(), LocalEligibility::kNotSignedIn);
  EXPECT_EQ(service_->GetLocalEligibility(), LocalEligibility::kNotSignedIn);
}

TEST_F(IndigoServiceTest, LoadPrompts) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a test proto.
  chrome::aix::indigo::IndigoPrompts proto;
  auto* prompt1 = proto.add_prompts();
  prompt1->set_key("v5");
  prompt1->set_prompt("Test prompt v5");
  auto* prompt2 = proto.add_prompts();
  prompt2->set_key("v6");
  prompt2->set_prompt("Test prompt v6");

  // Serialize to file.
  base::FilePath prompts_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("indigo_prompts.bin"));
  std::string serialized;
  ASSERT_TRUE(proto.SerializeToString(&serialized));
  ASSERT_TRUE(base::WriteFile(prompts_path, serialized));

  CreateService();

  // Initially prompts should not be loaded.
  EXPECT_EQ(service_->GetPrompt("v5"), std::nullopt);

  base::test::TestFuture<void> prompts_loaded_future;
  service_->SetPromptsLoadedCallbackForTesting(
      prompts_loaded_future.GetCallback());

  // Simulate component ready with the temp dir.
  component_updater::IndigoComponentInstallerPolicy policy;
  policy.ComponentReady(base::Version("1.0"), temp_dir.GetPath(),
                        base::DictValue());

  // Wait for the background task to load prompts.
  EXPECT_TRUE(prompts_loaded_future.Wait());

  // Verify prompts are loaded.
  EXPECT_EQ(service_->GetPrompt("v5"), "Test prompt v5");
  EXPECT_EQ(service_->GetPrompt("v6"), "Test prompt v6");
  EXPECT_EQ(service_->GetPrompt("non_existent"), std::nullopt);
}

TEST_F(IndigoServiceTest, LoadPromptsComponentAlreadyReady) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a test proto.
  chrome::aix::indigo::IndigoPrompts proto;
  auto* prompt1 = proto.add_prompts();
  prompt1->set_key("v5");
  prompt1->set_prompt("Test prompt v5");

  // Serialize to file.
  base::FilePath prompts_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("indigo_prompts.bin"));
  std::string serialized;
  ASSERT_TRUE(proto.SerializeToString(&serialized));
  ASSERT_TRUE(base::WriteFile(prompts_path, serialized));

  // Simulate component ready BEFORE service creation.
  component_updater::IndigoComponentInstallerPolicy policy;
  policy.ComponentReady(base::Version("1.0"), temp_dir.GetPath(),
                        base::DictValue());

  CreateService();

  // Verify prompts are loaded. Since the component was already ready,
  // the service should start loading them immediately.
  if (service_->GetPrompt("v5") != "Test prompt v5") {
    base::test::TestFuture<void> prompts_loaded_future;
    service_->SetPromptsLoadedCallbackForTesting(
        prompts_loaded_future.GetCallback());
    EXPECT_TRUE(prompts_loaded_future.Wait());
  }

  EXPECT_EQ(service_->GetPrompt("v5"), "Test prompt v5");
}

TEST_F(IndigoServiceTest, LoadConfig) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a test config.
  chrome::aix::indigo::IndigoConfig proto;
  auto* heuristic_config = proto.mutable_heuristic_config();
  heuristic_config->add_allowed_origins("https://allowed1.com");
  heuristic_config->add_allowed_origins("https://allowed2.com");
  heuristic_config->add_allowed_keywords("keyword1");
  heuristic_config->add_allowed_keywords("keyword2");
  heuristic_config->add_blocked_keywords("blocked1");
  heuristic_config->add_blocked_keywords("blocked2");

  // Serialize to file.
  base::FilePath config_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("indigo_config.bin"));
  std::string serialized;
  ASSERT_TRUE(proto.SerializeToString(&serialized));
  ASSERT_TRUE(base::WriteFile(config_path, serialized));

  CreateService();

  // Initially config should not be loaded.
  EXPECT_FALSE(service_->IsConfigLoaded());

  // Simulate component ready.
  component_updater::IndigoComponentInstallerPolicy policy;
  policy.ComponentReady(base::Version("1.0"), temp_dir.GetPath(),
                        base::DictValue());

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return service_->IsConfigLoaded(); }));

  // Verify config is loaded.
  EXPECT_TRUE(service_->IsConfigLoaded());
  EXPECT_TRUE(service_->IsOriginAllowed(
      url::Origin::Create(GURL("https://allowed1.com"))));
  EXPECT_TRUE(service_->IsOriginAllowed(
      url::Origin::Create(GURL("https://allowed2.com"))));
  EXPECT_FALSE(service_->IsOriginAllowed(
      url::Origin::Create(GURL("https://disallowed.com"))));

  EXPECT_THAT(service_->GetAllowedKeywords(),
              testing::ElementsAre("keyword1", "keyword2"));
  EXPECT_THAT(service_->GetBlockedKeywords(),
              testing::ElementsAre("blocked1", "blocked2"));
}

TEST_F(IndigoServiceTest, LoadConfigFromCommandLine) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create a test config.
  chrome::aix::indigo::IndigoConfig proto;
  auto* heuristic_config = proto.mutable_heuristic_config();
  heuristic_config->add_allowed_origins("https://allowed1.com");

  // Serialize to file.
  base::FilePath config_path =
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("indigo_config.bin"));
  std::string serialized;
  ASSERT_TRUE(proto.SerializeToString(&serialized));
  ASSERT_TRUE(base::WriteFile(config_path, serialized));

  // Set the command line switch.
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchPath(
      "indigo-config-proto", config_path);

  CreateService();

  // The service should start loading immediately because of the switch.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsConfigLoaded(); }));

  // Verify config is loaded.
  EXPECT_TRUE(service_->IsOriginAllowed(
      url::Origin::Create(GURL("https://allowed1.com"))));
}

TEST_F(IndigoServiceTest, ComponentUpdateReloadsPromptsAndConfig) {
  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());

  // Create initial prompt and config in temp_dir1.
  chrome::aix::indigo::IndigoPrompts prompts_proto1;
  auto* prompt1 = prompts_proto1.add_prompts();
  prompt1->set_key("v1");
  prompt1->set_prompt("Prompt 1");
  std::string serialized_prompts1;
  ASSERT_TRUE(prompts_proto1.SerializeToString(&serialized_prompts1));
  ASSERT_TRUE(base::WriteFile(
      temp_dir1.GetPath().Append(FILE_PATH_LITERAL("indigo_prompts.bin")),
      serialized_prompts1));

  chrome::aix::indigo::IndigoConfig config_proto1;
  config_proto1.mutable_heuristic_config()->add_allowed_origins(
      "https://allowed1.com");
  std::string serialized_config1;
  ASSERT_TRUE(config_proto1.SerializeToString(&serialized_config1));
  ASSERT_TRUE(base::WriteFile(
      temp_dir1.GetPath().Append(FILE_PATH_LITERAL("indigo_config.bin")),
      serialized_config1));

  CreateService();

  component_updater::IndigoComponentInstallerPolicy policy;
  policy.ComponentReady(base::Version("1.0"), temp_dir1.GetPath(),
                        base::DictValue());

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return service_->IsConfigLoaded(); }));
  if (service_->GetPrompt("v1") != "Prompt 1") {
    base::test::TestFuture<void> prompts_loaded_future;
    service_->SetPromptsLoadedCallbackForTesting(
        prompts_loaded_future.GetCallback());
    EXPECT_TRUE(prompts_loaded_future.Wait());
  }

  EXPECT_EQ(service_->GetPrompt("v1"), "Prompt 1");
  EXPECT_TRUE(service_->IsOriginAllowed(
      url::Origin::Create(GURL("https://allowed1.com"))));

  // Now simulate component update with version 2.0 in a new directory.
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  chrome::aix::indigo::IndigoPrompts prompts_proto2;
  auto* prompt2 = prompts_proto2.add_prompts();
  prompt2->set_key("v2");
  prompt2->set_prompt("Prompt 2");
  std::string serialized_prompts2;
  ASSERT_TRUE(prompts_proto2.SerializeToString(&serialized_prompts2));
  ASSERT_TRUE(base::WriteFile(
      temp_dir2.GetPath().Append(FILE_PATH_LITERAL("indigo_prompts.bin")),
      serialized_prompts2));

  chrome::aix::indigo::IndigoConfig config_proto2;
  config_proto2.mutable_heuristic_config()->add_allowed_origins(
      "https://allowed2.com");
  std::string serialized_config2;
  ASSERT_TRUE(config_proto2.SerializeToString(&serialized_config2));
  ASSERT_TRUE(base::WriteFile(
      temp_dir2.GetPath().Append(FILE_PATH_LITERAL("indigo_config.bin")),
      serialized_config2));

  policy.ComponentReady(base::Version("2.0"), temp_dir2.GetPath(),
                        base::DictValue());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return service_->IsOriginAllowed(
        url::Origin::Create(GURL("https://allowed2.com")));
  }));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return service_->GetPrompt("v2") == "Prompt 2"; }));

  EXPECT_FALSE(service_->IsOriginAllowed(
      url::Origin::Create(GURL("https://allowed1.com"))));
  EXPECT_EQ(service_->GetPrompt("v1"), std::nullopt);
}

class IndigoServiceManagementPolicyDefaultEnabledTest
    : public IndigoServiceTest,
      public testing::WithParamInterface<bool> {
 public:
  IndigoServiceManagementPolicyDefaultEnabledTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIndigo,
        {{features::kIndigoAllowForEnterprise.name,
          IsIndigoAllowedForEnterprise() ? "true" : "false"}});
  }

  bool IsIndigoAllowedForEnterprise() const { return GetParam(); }
};

TEST_P(IndigoServiceManagementPolicyDefaultEnabledTest,
       PolicyDisabledFromConstruction) {
  SetPolicySettings(prefs::Policy::kDisallowed);
  CreateService();
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(&profile_),
          policy::EnterpriseManagementAuthority::CLOUD);
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kDisabledByPolicy
                                  : LocalEligibility::kEnterpriseDisallowed));
}

TEST_P(IndigoServiceManagementPolicyDefaultEnabledTest,
       PolicyChangeTriggersUpdate) {
  CreateService();
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(&profile_),
          policy::EnterpriseManagementAuthority::CLOUD);
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kEligible
                                  : LocalEligibility::kEnterpriseDisallowed));

  SetPolicySettings(prefs::Policy::kDisallowed);
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kDisabledByPolicy
                                  : LocalEligibility::kEnterpriseDisallowed));
}

TEST_P(IndigoServiceManagementPolicyDefaultEnabledTest, ManagedDomain) {
  CreateService();
  MakeAccountAvailableAndCapable("test@example.com", "example.com");
  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kManagedDomain));
}

TEST_P(IndigoServiceManagementPolicyDefaultEnabledTest,
       GoogleInternalAccountNotManaged) {
  CreateService();
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(&profile_),
          policy::EnterpriseManagementAuthority::CLOUD);
  MakeAccountAvailableAndCapable("test@google.com", "google.com");

  EXPECT_TRUE(LocalEligibilityBecomes(LocalEligibility::kEligible));
}

TEST_P(IndigoServiceManagementPolicyDefaultEnabledTest, ManagedProfileCloud) {
  CreateService();
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(&profile_),
          policy::EnterpriseManagementAuthority::CLOUD);
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kEligible
                                  : LocalEligibility::kEnterpriseDisallowed));
}

TEST_P(IndigoServiceManagementPolicyDefaultEnabledTest,
       ManagedProfileComputerLocal) {
  CreateService();
  policy::ScopedManagementServiceOverrideForTesting
      scoped_management_service_override(
          policy::ManagementServiceFactory::GetForProfile(&profile_),
          policy::EnterpriseManagementAuthority::COMPUTER_LOCAL);
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kEligible
                                  : LocalEligibility::kEnterpriseDisallowed));
}

#if BUILDFLAG(IS_WIN)
TEST_P(IndigoServiceManagementPolicyDefaultEnabledTest, EnterpriseDeviceWin) {
  CreateService();
  auto scoped_device_override = base::SetIsEnterpriseDeviceForTesting(true);
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kEligible
                                  : LocalEligibility::kEnterpriseDisallowed));

  SetPolicySettings(prefs::Policy::kDisallowed);
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kDisabledByPolicy
                                  : LocalEligibility::kEnterpriseDisallowed));
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(IndigoServiceManagementPolicyDefaultEnabledTest,
       EnterpriseDeviceChromeOS) {
  CreateService();
  profile_.ScopedCrosSettingsTestHelper()->InstallAttributes()->SetCloudManaged(
      "example.com", "device_id");
  MakeAccountAvailableAndCapable();
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kEligible
                                  : LocalEligibility::kEnterpriseDisallowed));

  SetPolicySettings(prefs::Policy::kDisallowed);
  EXPECT_TRUE(
      LocalEligibilityBecomes(IsIndigoAllowedForEnterprise()
                                  ? LocalEligibility::kDisabledByPolicy
                                  : LocalEligibility::kEnterpriseDisallowed));
}
#endif

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IndigoServiceManagementPolicyDefaultEnabledTest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "AllowedForEnterprise" : "DisallowedForEnterprise";
    });

}  // namespace indigo
