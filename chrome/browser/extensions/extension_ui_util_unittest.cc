// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_ui_util.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/cws_info_service.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class ExtensionUIUtilMockCWSInfoService : public CWSInfoService {
 public:
  explicit ExtensionUIUtilMockCWSInfoService(Profile* profile)
      : CWSInfoService(profile) {}
  ~ExtensionUIUtilMockCWSInfoService() override = default;

  MOCK_METHOD(std::optional<CWSInfoServiceInterface::CWSInfo>,
              GetCWSInfo,
              (const Extension&),
              (const, override));
};

}  // namespace

class ExtensionUIUtilUnittest : public ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    ExtensionServiceInitParams params;
    params.prefs_content = "";
    params.testing_factories.emplace_back(
        CWSInfoServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          auto mock = std::make_unique<
              testing::NiceMock<ExtensionUIUtilMockCWSInfoService>>(
              Profile::FromBrowserContext(context));
          CWSInfoServiceInterface::CWSInfo live_info;
          live_info.is_present = true;
          live_info.is_live = true;
          live_info.violation_type =
              CWSInfoServiceInterface::CWSViolationType::kNone;
          ON_CALL(*mock, GetCWSInfo(testing::_))
              .WillByDefault(testing::Return(live_info));
          return mock;
        }));
    InitializeExtensionService(std::move(params));
  }
};

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_FeatureFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_ProfileTypes) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();

  // Regular profile qualifies.
  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Incognito/OffTheRecord profile returns false.
  Profile* incognito_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_FALSE(
      ui_util::ShouldShowReviewPrompt(*extension, *incognito_profile));

  // Guest profile returns false.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  std::unique_ptr<TestingProfile> guest_profile = guest_builder.Build();
  EXPECT_FALSE(
      ui_util::ShouldShowReviewPrompt(*extension, *guest_profile));
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_EnterprisePrefDisabled) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();

  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  profile()->GetPrefs()->SetBoolean(prefs::kExtensionReviewPromptsAllowed,
                                    false);
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_MustRemainInstalled) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();

  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  TestManagementPolicyProvider provider(
      TestManagementPolicyProvider::MUST_REMAIN_INSTALLED);
  ExtensionSystem::Get(profile())->management_policy()->RegisterProvider(
      &provider);

  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_ParentalControls) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(identity_manager);

  // An account must be signed in for parental controls to apply.
  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "user@example.com", signin::ConsentLevel::kSignin);
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_is_subject_to_parental_controls(false);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  mutator.set_is_subject_to_parental_controls(true);
  signin::UpdateAccountInfoForAccount(identity_manager, account_info);
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_ExtensionTypes) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  static constexpr struct {
    const char* name;
    mojom::ManifestLocation location;
    bool from_webstore;
    bool expect_review_prompt;
  } kTestCases[] = {
      {"internal_cws", mojom::ManifestLocation::kInternal, true, true},
      {"unpacked", mojom::ManifestLocation::kUnpacked, true, false},
      {"command_line", mojom::ManifestLocation::kCommandLine, true, false},
      {"component", mojom::ManifestLocation::kComponent, true, false},
      {"external_component", mojom::ManifestLocation::kExternalComponent,
       true, false},
      {"external_policy", mojom::ManifestLocation::kExternalPolicy, true,
       false},
      {"external_policy_download",
       mojom::ManifestLocation::kExternalPolicyDownload, true, false},
      {"internal_non_cws", mojom::ManifestLocation::kInternal, false, false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);
    ExtensionBuilder builder(test_case.name);
    builder.SetLocation(test_case.location);
    if (test_case.from_webstore) {
      builder.AddFlags(Extension::FROM_WEBSTORE);
    }
    scoped_refptr<const Extension> extension = builder.Build();
    EXPECT_EQ(test_case.expect_review_prompt,
              ui_util::ShouldShowReviewPrompt(*extension, *profile()));
  }
}

TEST_F(ExtensionUIUtilUnittest,
       ShouldShowReviewPrompt_CWSInfoServiceLiveStatus) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();

  auto* mock_cws_info =
      static_cast<testing::NiceMock<ExtensionUIUtilMockCWSInfoService>*>(
          CWSInfoServiceFactory::GetForProfile(profile()));

  using CWSInfo = CWSInfoServiceInterface::CWSInfo;
  using ViolationType = CWSInfoServiceInterface::CWSViolationType;

  enum class CWSState {
    kNoInfoCached,  // The CWS info cache has no entry for the extension.
    kNotInStore,    // Cached entry says the extension isn't in CWS.
    kUnpublished,   // In CWS, but not live (unpublished or taken down).
    kLive,          // In CWS and live.
  };
  enum class Expectation { kNoPrompt, kShowPrompt };

  static constexpr struct {
    const char* name;
    CWSState cws_state;
    ViolationType violation_type;
    Expectation expectation;
  } kTestCases[] = {
      // Fail closed unless CWS confirms the extension is live.
      {"no_info", CWSState::kNoInfoCached, ViolationType::kNone,
       Expectation::kNoPrompt},
      {"not_in_store", CWSState::kNotInStore, ViolationType::kNone,
       Expectation::kNoPrompt},
      {"unpublished", CWSState::kUnpublished, ViolationType::kNone,
       Expectation::kNoPrompt},

      // Taken down from CWS, so no longer live.
      {"malware_takedown", CWSState::kUnpublished, ViolationType::kMalware,
       Expectation::kNoPrompt},
      {"policy_takedown", CWSState::kUnpublished, ViolationType::kPolicy,
       Expectation::kNoPrompt},

      // The only eligible state.
      {"live_no_violation", CWSState::kLive, ViolationType::kNone,
       Expectation::kShowPrompt},

      // CWS is not expected to report a violation while the extension is still
      // live, but the prompt must stay suppressed if it does. kUnknown covers
      // violation types added to the server before this client parses them.
      {"live_malware", CWSState::kLive, ViolationType::kMalware,
       Expectation::kNoPrompt},
      {"live_policy", CWSState::kLive, ViolationType::kPolicy,
       Expectation::kNoPrompt},
      {"live_minor_policy", CWSState::kLive, ViolationType::kMinorPolicy,
       Expectation::kNoPrompt},
      {"live_unknown", CWSState::kLive, ViolationType::kUnknown,
       Expectation::kNoPrompt},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);

    std::optional<CWSInfo> info;
    if (test_case.cws_state != CWSState::kNoInfoCached) {
      CWSInfo cws_info;
      cws_info.is_present = test_case.cws_state != CWSState::kNotInStore;
      cws_info.is_live = test_case.cws_state == CWSState::kLive;
      cws_info.violation_type = test_case.violation_type;
      info = cws_info;
    }

    EXPECT_CALL(*mock_cws_info, GetCWSInfo(testing::_))
        .WillRepeatedly(testing::Return(info));
    EXPECT_EQ(test_case.expectation == Expectation::kShowPrompt,
              ui_util::ShouldShowReviewPrompt(*extension, *profile()));

    testing::Mock::VerifyAndClearExpectations(mock_cws_info);
  }
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_TerminatedExtension) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();
  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Terminated extensions are offered a Reload affordance instead.
  registry()->AddTerminated(extension);
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_CorruptedExtension) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();
  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Corrupted extensions are offered a Repair affordance instead.
  ExtensionPrefs::Get(profile())->AddDisableReason(
      extension->id(), disable_reason::DISABLE_CORRUPTED);
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_SuspiciousInstall) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();
  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Extensions disabled as possibly installed without consent are ineligible.
  ExtensionPrefs::Get(profile())->AddDisableReason(
      extension->id(), disable_reason::DISABLE_NOT_VERIFIED);
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
}

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_BlocklistStates) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  static constexpr struct {
    const char* name;
    BitMapBlocklistState state;
  } kTestCases[] = {
      {"malware", BitMapBlocklistState::BLOCKLISTED_MALWARE},
      {"security_vulnerability",
       BitMapBlocklistState::BLOCKLISTED_SECURITY_VULNERABILITY},
      {"cws_policy_violation",
       BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION},
      {"potentially_unwanted",
       BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.name);

    // Each case uses a distinct extension id so the prefs start clean.
    scoped_refptr<const Extension> extension =
        ExtensionBuilder(test_case.name)
            .SetLocation(mojom::ManifestLocation::kInternal)
            .AddFlags(Extension::FROM_WEBSTORE)
            .Build();
    EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

    blocklist_prefs::AddOmahaBlocklistState(extension->id(), test_case.state,
                                            ExtensionPrefs::Get(profile()));
    EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
  }
}

}  // namespace extensions

