// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_safety_check_utils.h"

#include "base/strings/string_util.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/cws_info_service.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

class MockCWSInfoService : public CWSInfoServiceInterface {
 public:
  MOCK_METHOD(std::optional<bool>,
              IsLiveInCWS,
              (const Extension&),
              (const, override));
  MOCK_METHOD(std::optional<CWSInfoServiceInterface::CWSInfo>,
              GetCWSInfo,
              (const Extension&),
              (const, override));
  MOCK_METHOD(void, CheckAndMaybeFetchInfo, (), (override));
  MOCK_METHOD(void,
              AddObserver,
              (CWSInfoServiceInterface::Observer*),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (CWSInfoServiceInterface::Observer*),
              (override));

  static CWSInfoService::CWSInfo GetCWSInfoNone() {
    return CWSInfoService::CWSInfo{
        /*is_present=*/true,
        /*is_live=*/true,
        /*last_update_time=*/base::Time::Now(),
        /*violation_type=*/
        extensions::CWSInfoService::CWSViolationType::kNone,
        /*unpublished_long_ago=*/false,
        /*no_privacy_practice=*/false};
  }
  static CWSInfoService::CWSInfo GetCWSInfoMalware() {
    return CWSInfoService::CWSInfo{
        /*is_present=*/true,
        /*is_live=*/false,
        /*last_update_time=*/base::Time::Now(),
        /*violation_type=*/
        extensions::CWSInfoService::CWSViolationType::kMalware,
        /*unpublished_long_ago=*/false,
        /*no_privacy_practice=*/false};
  }
  static CWSInfoService::CWSInfo GetCWSInfoNoPrivacyPractice() {
    return CWSInfoService::CWSInfo{
        /*is_present=*/true,
        /*is_live=*/false,
        /*last_update_time=*/base::Time::Now(),
        /*violation_type=*/
        extensions::CWSInfoService::CWSViolationType::kNone,
        /*unpublished_long_ago=*/false,
        /*no_privacy_practice=*/true};
  }
  static CWSInfoService::CWSInfo GetCWSInfoPolicy() {
    return CWSInfoService::CWSInfo{
        /*is_present=*/true,
        /*is_live=*/false,
        /*last_update_time=*/base::Time::Now(),
        /*violation_type=*/
        extensions::CWSInfoService::CWSViolationType::kPolicy,
        /*unpublished_long_ago=*/false,
        /*no_privacy_practice=*/false};
  }
  static CWSInfoService::CWSInfo GetCWSInfoUnpublished() {
    return CWSInfoService::CWSInfo{
        /*is_present=*/true,
        /*is_live=*/false,
        /*last_update_time=*/base::Time::Now(),
        /*violation_type=*/
        extensions::CWSInfoService::CWSViolationType::kNone,
        /*unpublished_long_ago=*/true,
        /*no_privacy_practice=*/false};
  }
};

const scoped_refptr<const Extension> CreateExtension(
    const std::string& name,
    base::Value::List permissions,
    mojom::ManifestLocation location,
    const std::string& update_url = extension_urls::kChromeWebstoreUpdateURL) {
  const ExtensionId kId = crx_file::id_util::GenerateId(name);
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", name)
                           .Set("description", "an extension")
                           .Set("manifest_version", 2)
                           .Set("version", "1.0.0")
                           .Set("permissions", std::move(permissions))
                           .Set("update_url", update_url))
          .SetLocation(location)
          .SetID(kId)
          .Build();

  return extension;
}

// Ensures that the warning_reason and the safety check strings match.
void CheckSafetyCheckDisplayString(
    api::developer_private::SafetyCheckWarningReason warning_reason,
    api::developer_private::SafetyCheckStrings strings,
    bool extension_state = true) {
  int detail_page_string = 0;
  int panel_string = 0;
  switch (warning_reason) {
    case api::developer_private::SafetyCheckWarningReason::kMalware:
      detail_page_string = IDS_SAFETY_CHECK_EXTENSIONS_MALWARE;
      panel_string = IDS_EXTENSIONS_SC_MALWARE;
      break;
    case api::developer_private::SafetyCheckWarningReason::kPolicy:
      detail_page_string = IDS_SAFETY_CHECK_EXTENSIONS_POLICY_VIOLATION;
      panel_string = extension_state ? IDS_EXTENSIONS_SC_POLICY_VIOLATION_ON
                                     : IDS_EXTENSIONS_SC_POLICY_VIOLATION_OFF;
      break;
    case api::developer_private::SafetyCheckWarningReason::kUnpublished:
      detail_page_string = IDS_SAFETY_CHECK_EXTENSIONS_UNPUBLISHED;
      panel_string = extension_state ? IDS_EXTENSIONS_SC_UNPUBLISHED_ON
                                     : IDS_EXTENSIONS_SC_UNPUBLISHED_OFF;
      break;
    case api::developer_private::SafetyCheckWarningReason::kOffstore:
      detail_page_string = IDS_EXTENSIONS_SAFETY_CHECK_OFFSTORE;
      panel_string = extension_state ? IDS_EXTENSIONS_SAFETY_CHECK_OFFSTORE_ON
                                     : IDS_EXTENSIONS_SAFETY_CHECK_OFFSTORE_OFF;
      break;
    case api::developer_private::SafetyCheckWarningReason::kUnwanted:
      detail_page_string = IDS_SAFETY_CHECK_EXTENSIONS_POLICY_VIOLATION;
      panel_string = extension_state ? IDS_EXTENSIONS_SC_POLICY_VIOLATION_ON
                                     : IDS_EXTENSIONS_SC_POLICY_VIOLATION_OFF;
      break;
    case api::developer_private::SafetyCheckWarningReason::kNoPrivacyPractice:
      detail_page_string = IDS_EXTENSIONS_SAFETY_CHECK_NO_PRIVACY_PRACTICES;
      panel_string = extension_state
                         ? IDS_EXTENSIONS_SAFETY_CHECK_NO_PRIVACY_PRACTICES_ON
                         : IDS_EXTENSIONS_SAFETY_CHECK_NO_PRIVACY_PRACTICES_OFF;
      break;
    case api::developer_private::SafetyCheckWarningReason::kNone:
      EXPECT_FALSE(strings.detail_string.has_value());
      EXPECT_FALSE(strings.panel_string.has_value());
      return;
  }
  EXPECT_EQ(strings.detail_string,
            l10n_util::GetStringUTF8(detail_page_string));
  EXPECT_EQ(strings.panel_string, l10n_util::GetStringUTF8(panel_string));
}

class SafetyCheckExtensionUtilsTest : public testing::Test {
 public:
  SafetyCheckExtensionUtilsTest(const SafetyCheckExtensionUtilsTest&) = delete;
  SafetyCheckExtensionUtilsTest& operator=(
      const SafetyCheckExtensionUtilsTest&) = delete;

  SafetyCheckExtensionUtilsTest() = default;
  ~SafetyCheckExtensionUtilsTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    feature_list_.InitWithFeatures(
        {features::kSafetyHubExtensionsUwSTrigger,
         features::kSafetyHubExtensionsOffStoreTrigger,
         features::kSafetyHubExtensionsNoPrivacyPracticesTrigger},
        /*disabled_features=*/{});
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockCWSInfoService> mock_cws_info_service_;
};

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_Malware) {
  // Test that a malware extension will trigger the Extension
  // Review Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::Value::List(), mojom::ManifestLocation::kInternal);
  {
    // CWSInfo - Malware.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoMalware()));
    api::developer_private::SafetyCheckWarningReason malware_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension);
    // A malware warning will be shown since the CWS info service returns
    // a malware trigger.
    EXPECT_EQ(malware_warning,
              api::developer_private::SafetyCheckWarningReason::kMalware);
  }
  {
    // Blocklist - Malware.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    api::developer_private::SafetyCheckWarningReason malware_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::BLOCKLISTED_MALWARE,
            profile_.get(), *extension);
    // A malware warning will be shown since the blocklist bit returns
    // malware.
    EXPECT_EQ(malware_warning,
              api::developer_private::SafetyCheckWarningReason::kMalware);
  }
  {
    // Blocklist - Malware - Kept for minor violation.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Unpublished Warning Reason*/ 1);
    api::developer_private::SafetyCheckWarningReason malware_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::BLOCKLISTED_MALWARE,
            profile_.get(), *extension);
    // A malware warning will be shown since the previously acknowledged
    // warning has a lesser warning level.
    EXPECT_EQ(malware_warning,
              api::developer_private::SafetyCheckWarningReason::kMalware);
  }
  {
    // Blocklist - Malware - Kept for Malware.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Malware Warning Reason*/ 3);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::BLOCKLISTED_MALWARE,
            profile_.get(), *extension);
    // No warning will be shown since the previously acknowledged
    // warning was malware.
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_PrefMigration) {
  // Test that the `PrefAcknowledgeSafetyCheckWarningReason` migration
  // works as intended
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::Value::List(), mojom::ManifestLocation::kInternal);
  {
    // Verify that the boolean `kPrefAcknowledgeSafetyCheckWarning` is
    // deleted if the `kPrefAcknowledgeSafetyCheckWarningReason` warning
    // reason pref is already present, and that the
    // `kPrefAcknowledgeSafetyCheckWarningReason` is unchanged.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Policy Warning Reason*/ 2);
    ExtensionPrefs::Get(profile_.get())
        ->SetBooleanPref(extension->id(), kPrefAcknowledgeSafetyCheckWarning,
                         true);
    ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
        &mock_cws_info_service_, BitMapBlocklistState::BLOCKLISTED_MALWARE,
        profile_.get(), *extension);
    bool warning = false;
    int warning_reason = 0;
    EXPECT_FALSE(
        ExtensionPrefs::Get(profile_.get())
            ->ReadPrefAsBoolean(extension->id(),
                                extensions::kPrefAcknowledgeSafetyCheckWarning,
                                &warning));
    EXPECT_TRUE(ExtensionPrefs::Get(profile_.get())
                    ->ReadPrefAsInteger(
                        extension->id(),
                        extensions::kPrefAcknowledgeSafetyCheckWarningReason,
                        &warning_reason));
    EXPECT_EQ(warning_reason, /*Policy Warning Reason*/ 2);
  }
  {
    // Verify that if only the bool `kPrefAcknowledgeSafetyCheckWarning`
    // is present, then:
    //   - The boolean pref is deleted
    //   - The `kPrefAcknowledgeSafetyCheckWarningReason` pref is added with
    //     the current warning reason
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    ExtensionPrefs::Get(profile_.get())
        ->SetBooleanPref(extension->id(), kPrefAcknowledgeSafetyCheckWarning,
                         true);
    ExtensionPrefs::Get(profile_.get())
        ->UpdateExtensionPref(
            extension->id(),
            extensions::kPrefAcknowledgeSafetyCheckWarningReason.name,
            std::nullopt);
    ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
        &mock_cws_info_service_, BitMapBlocklistState::BLOCKLISTED_MALWARE,
        profile_.get(), *extension);
    bool warning = false;
    EXPECT_FALSE(
        ExtensionPrefs::Get(profile_.get())
            ->ReadPrefAsBoolean(extension->id(),
                                extensions::kPrefAcknowledgeSafetyCheckWarning,
                                &warning));
    int warning_reason = 0;
    EXPECT_TRUE(ExtensionPrefs::Get(profile_.get())
                    ->ReadPrefAsInteger(
                        extension->id(),
                        extensions::kPrefAcknowledgeSafetyCheckWarningReason,
                        &warning_reason));
    EXPECT_EQ(warning_reason, /*Malware Warning Reason*/ 3);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_NoPrivacyPractice) {
  // Test that a extension without proper privacy practices will trigger
  // the Extension Review Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::Value::List(), mojom::ManifestLocation::kInternal);
  {
    // CWSInfo - No Privacy Practice.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(
            testing::Return(MockCWSInfoService::GetCWSInfoNoPrivacyPractice()));
    api::developer_private::SafetyCheckWarningReason no_privacy_practice =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension);
    // A privacy practice warning will be shown since the CWS info service
    // returns a privacy practice trigger.
    EXPECT_EQ(
        no_privacy_practice,
        api::developer_private::SafetyCheckWarningReason::kNoPrivacyPractice);
  }
  {
    // CWSInfo - No Privacy Practice kept for Policy.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(
            testing::Return(MockCWSInfoService::GetCWSInfoNoPrivacyPractice()));
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Policy Warning Reason*/ 2);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension);
    // No warning will be shown since the previously acknowledged
    // warning has a higher warning level.
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // CWSInfo - No Privacy Practice.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(
            testing::Return(MockCWSInfoService::GetCWSInfoNoPrivacyPractice()));
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Offstore Warning Reason*/ 4);
    api::developer_private::SafetyCheckWarningReason no_privacy_practice =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension);
    // A warning will be shown since the previously acknowledged
    // warning has a lower warning level.
    EXPECT_EQ(
        no_privacy_practice,
        api::developer_private::SafetyCheckWarningReason::kNoPrivacyPractice);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_OffStore) {
  // Test that a off store extension will trigger the Extension Review
  // Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension_unpacked = CreateExtension(
      "test2", base::Value::List(), mojom::ManifestLocation::kUnpacked);
  {
    // CWSInfo - No Trigger - Unpacked extension not in dev mode.
    profile_.get()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode,
                                           false);
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    api::developer_private::SafetyCheckWarningReason offstore =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension_unpacked);
    // A warning will be shown since the extension was unpacked and
    // Chrome is not in dev mode.
    EXPECT_EQ(offstore,
              api::developer_private::SafetyCheckWarningReason::kOffstore);
  }
  {
    // CWSInfo - No Trigger - Unpacked extension in dev mode.
    profile_.get()->GetPrefs()->SetBoolean(prefs::kExtensionsUIDeveloperMode,
                                           true);
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension_unpacked);
    // No warning will be shown since the extension was unpacked and
    // Chrome is in dev mode.
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // CWSInfo - No Trigger - Extension does not update from the webstore.
    const scoped_refptr<const Extension> extension_not_webstore =
        CreateExtension("test", base::Value::List(),
                        mojom::ManifestLocation::kInternal,
                        "https://example.com");
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    api::developer_private::SafetyCheckWarningReason offstore =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension_not_webstore);
    // A warning will be shown since the extension does not update
    // from the Chrome web store.
    EXPECT_EQ(offstore,
              api::developer_private::SafetyCheckWarningReason::kOffstore);
  }
  {
    // CWSInfo - Normal extension without CWS info.
    const scoped_refptr<const Extension> extension_normal = CreateExtension(
        "test", base::Value::List(), mojom::ManifestLocation::kInternal);
    CWSInfoService::CWSInfo cws_not_present;
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(cws_not_present));
    api::developer_private::SafetyCheckWarningReason offstore =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension_normal);
    // A warning will be shown since the CWS info service does not return
    // any information on an extension.
    EXPECT_EQ(offstore,
              api::developer_private::SafetyCheckWarningReason::kOffstore);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_Policy) {
  // Test that a extension with a policy violation will trigger
  // the Extension Review Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::Value::List(), mojom::ManifestLocation::kInternal);
  {
    // CWSInfo - Policy.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoPolicy()));
    api::developer_private::SafetyCheckWarningReason policy_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension);
    // A warning will be shown since the CWS info service returns a policy
    // warning.
    EXPECT_EQ(policy_warning,
              api::developer_private::SafetyCheckWarningReason::kPolicy);
  }
  {
    // Blocklist - Policy.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    api::developer_private::SafetyCheckWarningReason policy_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension);
    // A policy warning will be shown since the previously acknowledged
    // warning has a lesser warning level.
    EXPECT_EQ(policy_warning,
              api::developer_private::SafetyCheckWarningReason::kPolicy);
  }
  {
    // Blocklist - Policy - Kept for minor violation.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Unpublished Warning Reason*/ 1);
    api::developer_private::SafetyCheckWarningReason policy_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension);
    // A policy warning will be shown since the previously acknowledged
    // warning has a lesser warning level.
    EXPECT_EQ(policy_warning,
              api::developer_private::SafetyCheckWarningReason::kPolicy);
  }
  {
    // Blocklist - Policy - Kept for Malware.
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    ExtensionPrefs::Get(profile_.get())
        ->SetIntegerPref(extension->id(),
                         kPrefAcknowledgeSafetyCheckWarningReason,
                         /*Malware Warning Reason*/ 3);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension);
    // No warning will be shown since the previously acknowledged
    // warning has a higher warning level.
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_PotentiallyUnwanted) {
  // Test that a potentially unwanted extension will trigger the Extension
  // Review Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::Value::List(), mojom::ManifestLocation::kInternal);
  // Blocklist - Potentially unwanted.
  EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
      .Times(1)
      .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
  api::developer_private::SafetyCheckWarningReason unwanted =
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
          &mock_cws_info_service_,
          BitMapBlocklistState::BLOCKLISTED_POTENTIALLY_UNWANTED,
          profile_.get(), *extension);
  // A warning will be shown since the extension has an unwanted blocklist
  // bit flipped.
  EXPECT_EQ(unwanted,
            api::developer_private::SafetyCheckWarningReason::kUnwanted);
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_Unpublished) {
  // Test that a unpublished extension will trigger the Extension
  // Review Panel based on if it has been kept or not.
  const scoped_refptr<const Extension> extension = CreateExtension(
      "test", base::Value::List(), mojom::ManifestLocation::kInternal);
  // Blocklist -  unpublished.
  EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
      .Times(1)
      .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoUnpublished()));
  api::developer_private::SafetyCheckWarningReason unpublished =
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
          &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
          profile_.get(), *extension);
  // A warning will be shown since the CWS info service returns a
  // unpublished warning.
  EXPECT_EQ(unpublished,
            api::developer_private::SafetyCheckWarningReason::kUnpublished);
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_No_Warning) {
  {
    // Test that no warning is shown for an extension without any store
    // violations.
    const scoped_refptr<const Extension> extension = CreateExtension(
        "test", base::Value::List(), mojom::ManifestLocation::kInternal);
    EXPECT_CALL(mock_cws_info_service_, GetCWSInfo(testing::_))
        .Times(1)
        .WillOnce(testing::Return(MockCWSInfoService::GetCWSInfoNone()));
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_, BitMapBlocklistState::NOT_BLOCKLISTED,
            profile_.get(), *extension);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // Test that no warning is shown for a component extension.
    const scoped_refptr<const Extension> extension_component_location =
        CreateExtension("test", base::Value::List(),
                        mojom::ManifestLocation::kComponent);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension_component_location);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // Test that an extension explicitly allowed by policy does not
    // trigger a review panel warning.
    using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>;
    const scoped_refptr<const Extension> extension_policy_location =
        CreateExtension("test", base::Value::List(),
                        mojom::ManifestLocation::kInternal);
    sync_preferences::TestingPrefServiceSyncable* prefs =
        profile_->GetTestingPrefService();
    PolicyUpdater(prefs).SetIndividualExtensionInstallationAllowed(
        extension_policy_location->id(), true);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension_policy_location);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
  {
    // Test than an extension force-installed by policy does not trigger a
    // review panel warning.
    using PolicyUpdater = extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>;
    const scoped_refptr<const Extension> extension_policy_location =
        CreateExtension("test", base::Value::List(),
                        mojom::ManifestLocation::kInternal);
    sync_preferences::TestingPrefServiceSyncable* prefs =
        profile_->GetTestingPrefService();
    PolicyUpdater(prefs).SetIndividualExtensionAutoInstalled(
        extension_policy_location->id(),
        extension_urls::kChromeWebstoreUpdateURL, true);
    api::developer_private::SafetyCheckWarningReason no_warning =
        ExtensionSafetyCheckUtils::GetSafetyCheckWarningReasonHelper(
            &mock_cws_info_service_,
            BitMapBlocklistState::BLOCKLISTED_CWS_POLICY_VIOLATION,
            profile_.get(), *extension_policy_location);
    EXPECT_EQ(no_warning,
              api::developer_private::SafetyCheckWarningReason::kNone);
  }
}

TEST_F(SafetyCheckExtensionUtilsTest, SafetyCheck_String_Check) {
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kUnpublished,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kUnpublished,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kMalware,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kMalware,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kPolicy,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kPolicy,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kOffstore,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kOffstore,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kUnpublished,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kUnpublished,
          api::developer_private::ExtensionState::kDisabled),
      false);
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kNoPrivacyPractice,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kNoPrivacyPractice,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kUnwanted,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kUnwanted,
          api::developer_private::ExtensionState::kEnabled));
  CheckSafetyCheckDisplayString(
      api::developer_private::SafetyCheckWarningReason::kNone,
      ExtensionSafetyCheckUtils::GetSafetyCheckWarningStrings(
          api::developer_private::SafetyCheckWarningReason::kNone,
          api::developer_private::ExtensionState::kEnabled));
}
}  // namespace extensions
