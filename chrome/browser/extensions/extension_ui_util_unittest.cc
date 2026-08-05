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
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/cws_info_service.h"
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

TEST_F(ExtensionUIUtilUnittest, ShouldShowReviewPrompt_FeatureFlagEnabled) {
  base::test::ScopedFeatureList feature_list(
      extensions_features::kCWSReviewPromptingNativeUI);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("cws_ext")
          .SetLocation(mojom::ManifestLocation::kInternal)
          .AddFlags(Extension::FROM_WEBSTORE)
          .Build();
  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
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

  // Unpopulated CWS info cache returns false (fail-closed).
  EXPECT_CALL(*mock_cws_info, GetCWSInfo(testing::_))
      .WillRepeatedly(testing::Return(std::nullopt));
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Malware violation returns false.
  CWSInfoServiceInterface::CWSInfo malware_info;
  malware_info.is_present = true;
  malware_info.violation_type =
      CWSInfoServiceInterface::CWSViolationType::kMalware;
  EXPECT_CALL(*mock_cws_info, GetCWSInfo(testing::_))
      .WillRepeatedly(testing::Return(malware_info));
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Policy violation returns false.
  CWSInfoServiceInterface::CWSInfo policy_info;
  policy_info.is_present = true;
  policy_info.violation_type =
      CWSInfoServiceInterface::CWSViolationType::kPolicy;
  EXPECT_CALL(*mock_cws_info, GetCWSInfo(testing::_))
      .WillRepeatedly(testing::Return(policy_info));
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Live extension with no violation returns true.
  CWSInfoServiceInterface::CWSInfo live_info;
  live_info.is_present = true;
  live_info.is_live = true;
  live_info.violation_type =
      CWSInfoServiceInterface::CWSViolationType::kNone;
  EXPECT_CALL(*mock_cws_info, GetCWSInfo(testing::_))
      .WillRepeatedly(testing::Return(live_info));
  EXPECT_TRUE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Non-live (unpublished) extension returns false.
  CWSInfoServiceInterface::CWSInfo non_live_info;
  non_live_info.is_present = true;
  non_live_info.is_live = false;
  non_live_info.violation_type =
      CWSInfoServiceInterface::CWSViolationType::kNone;
  EXPECT_CALL(*mock_cws_info, GetCWSInfo(testing::_))
      .WillRepeatedly(testing::Return(non_live_info));
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));

  // Missing store catalog extension returns false.
  CWSInfoServiceInterface::CWSInfo not_present_info;
  not_present_info.is_present = false;
  not_present_info.is_live = false;
  not_present_info.violation_type =
      CWSInfoServiceInterface::CWSViolationType::kNone;
  EXPECT_CALL(*mock_cws_info, GetCWSInfo(testing::_))
      .WillRepeatedly(testing::Return(not_present_info));
  EXPECT_FALSE(ui_util::ShouldShowReviewPrompt(*extension, *profile()));
}

}  // namespace extensions

