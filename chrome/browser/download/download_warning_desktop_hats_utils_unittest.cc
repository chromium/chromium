// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_warning_desktop_hats_utils.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/download/download_item_warning_data.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#endif

namespace {

using download::DownloadItem;
using download::MockDownloadItem;
using Fields = DownloadWarningHatsProductSpecificData::Fields;
using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Key;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::UnorderedElementsAreArray;

constexpr char kUrl[] = "https://www.site.example/file.pdf";
constexpr char kReferrerUrl[] = "https://www.site.example/referrer";
constexpr char kPlaceholderPrefix[] = "Not logged";
const base::FilePath::CharType kFilename[] = FILE_PATH_LITERAL("my_file.pdf");
constexpr base::TimeDelta kIgnoreDelay = base::Seconds(10);

// Matcher that checks for the presence of a particular bits data field and
// checks that the field value matches the given matcher.
MATCHER_P2(BitsDataMatches, field, matcher, "") {
  const auto it = arg.bits_data().find(field);
  if (it == arg.bits_data().end()) {
    return false;
  }
  return ExplainMatchResult(matcher, it->second, result_listener);
}

// As above, but for string data.
MATCHER_P2(StringDataMatches, field, matcher, "") {
  const auto it = arg.string_data().find(field);
  if (it == arg.string_data().end()) {
    return false;
  }
  return ExplainMatchResult(matcher, it->second, result_listener);
}

// Checks that the `fields` (vector of strings) exactly matches the keys in the
// `arg` (map of string->T).
MATCHER_P(UnorderedKeysAre, fields, "") {
  std::vector<std::string> keys;
  for (const auto& [key, val] : arg) {
    keys.push_back(key);
  }
  return ExplainMatchResult(UnorderedElementsAreArray(fields), keys,
                            result_listener);
}

class DownloadWarningDesktopHatsUtilsTest : public ::testing::Test {
 public:
  DownloadWarningDesktopHatsUtilsTest() {
    features_.InitAndEnableFeature(safe_browsing::kDownloadTailoredWarnings);
  }

  ~DownloadWarningDesktopHatsUtilsTest() override = default;

  void SetUp() override {
    profile_ = TestingProfile::Builder().Build();
    item_ = SetUpMockDownloadItem();
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), base::BindRepeating(&BuildMockHatsService)));
  }

  void TearDown() override { mock_hats_service_ = nullptr; }

  std::unique_ptr<NiceMock<MockDownloadItem>> SetUpMockDownloadItem() {
    auto item = std::make_unique<NiceMock<MockDownloadItem>>();
    content::DownloadItemUtils::AttachInfoForTesting(item.get(), profile_.get(),
                                                     nullptr);
    SetUpDefaultsForItem(item.get());
    return item;
  }

  // Sets up defaults for the mock download item_. These are not necessarily
  // valid/consistent with the Safe Browsing state, but we just want to test
  // that the values are reflected in the PSD and don't necessarily care what
  // they are.
  // Advances the time by 7 seconds.
  void SetUpDefaultsForItem(MockDownloadItem* item) {
    ON_CALL(*item, GetURL()).WillByDefault(ReturnRefOfCopy(GURL(kUrl)));
    ON_CALL(*item, GetReferrerUrl())
        .WillByDefault(ReturnRefOfCopy(GURL(kReferrerUrl)));
    ON_CALL(*item, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath(kFilename)));

    // Set up the time since download started.
    base::Time start_time = base::Time::Now();
    ON_CALL(*item, GetStartTime()).WillByDefault(Return(start_time));

    // Add some warning action events.
    task_environment_.FastForwardBy(base::Seconds(5));
    DownloadItemWarningData::AddWarningActionEvent(
        item, DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE,
        DownloadItemWarningData::WarningAction::SHOWN);
    task_environment_.FastForwardBy(base::Seconds(1));
    DownloadItemWarningData::AddWarningActionEvent(
        item, DownloadItemWarningData::WarningSurface::BUBBLE_MAINPAGE,
        DownloadItemWarningData::WarningAction::OPEN_SUBPAGE);
    task_environment_.FastForwardBy(base::Seconds(1));
    DownloadItemWarningData::AddWarningActionEvent(
        item, DownloadItemWarningData::WarningSurface::BUBBLE_SUBPAGE,
        DownloadItemWarningData::WarningAction::CLOSE);

    ON_CALL(*item, IsDone()).WillByDefault(Return(false));
    ON_CALL(*item, IsDangerous()).WillByDefault(Return(true));
    ON_CALL(*item, GetDangerType())
        .WillByDefault(
            Return(download::DownloadDangerType::
                       DOWNLOAD_DANGER_TYPE_DANGEROUS_ACCOUNT_COMPROMISE));

#if BUILDFLAG(FULL_SAFE_BROWSING)
    // Set tailored verdict for cookie theft with account info.
    safe_browsing::ClientDownloadResponse::TailoredVerdict tailored_verdict;
    tailored_verdict.set_tailored_verdict_type(
        safe_browsing::ClientDownloadResponse::TailoredVerdict::COOKIE_THEFT);
    tailored_verdict.add_adjustments(safe_browsing::ClientDownloadResponse::
                                         TailoredVerdict::ACCOUNT_INFO_STRING);
    safe_browsing::DownloadProtectionService::SetDownloadProtectionData(
        item, "token",
        safe_browsing::ClientDownloadResponse::DANGEROUS_ACCOUNT_COMPROMISE,
        std::move(tailored_verdict));
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

    ON_CALL(*item, HasUserGesture()).WillByDefault(Return(true));
  }

  void ExpectDefaultPsd(const DownloadWarningHatsProductSpecificData& psd) {
    EXPECT_THAT(psd,
                StringDataMatches(Fields::kSecondsSinceDownloadStarted, "7"));
    EXPECT_THAT(psd, StringDataMatches(Fields::kSecondsSinceWarningShown, "2"));
    EXPECT_THAT(psd, StringDataMatches(Fields::kDangerType,
                                       HasSubstr("AccountCompromise")));
#if BUILDFLAG(FULL_SAFE_BROWSING)
    EXPECT_THAT(psd,
                StringDataMatches(Fields::kDangerType,
                                  HasSubstr("Cookie theft with account info")));
#endif
    EXPECT_THAT(psd, StringDataMatches(Fields::kWarningType, "Dangerous"));
    EXPECT_THAT(psd, BitsDataMatches(Fields::kUserGesture, true));
    EXPECT_THAT(psd, BitsDataMatches(Fields::kPartialViewEnabled, true));
  }

  void ExpectDefaultPsdForSafeBrowsing(
      const DownloadWarningHatsProductSpecificData& psd) {
    EXPECT_THAT(psd, StringDataMatches(Fields::kUrlDownload, kUrl));
    EXPECT_THAT(psd, StringDataMatches(Fields::kUrlReferrer, kReferrerUrl));
    EXPECT_THAT(psd, StringDataMatches(Fields::kFilename, "my_file.pdf"));
  }

  void ExpectPlaceholderForSafeBrowsing(
      const DownloadWarningHatsProductSpecificData& psd) {
    EXPECT_THAT(psd, StringDataMatches(Fields::kUrlDownload,
                                       HasSubstr(kPlaceholderPrefix)));
    EXPECT_THAT(psd, StringDataMatches(Fields::kUrlReferrer,
                                       HasSubstr(kPlaceholderPrefix)));
    EXPECT_THAT(psd, StringDataMatches(Fields::kFilename,
                                       HasSubstr(kPlaceholderPrefix)));
  }

  void ExpectDefaultPsdForEnhancedSafeBrowsing(
      const DownloadWarningHatsProductSpecificData& psd) {
    EXPECT_THAT(
        psd, StringDataMatches(Fields::kWarningInteractions,
                               "BUBBLE_MAINPAGE:SHOWN:0,BUBBLE_MAINPAGE:OPEN_"
                               "SUBPAGE:1000,BUBBLE_SUBPAGE:CLOSE:2000"));
  }

  void ExpectPlaceholderForEnhancedSafeBrowsing(
      const DownloadWarningHatsProductSpecificData& psd) {
    EXPECT_THAT(psd, StringDataMatches(Fields::kWarningInteractions,
                                       HasSubstr(kPlaceholderPrefix)));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList features_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<NiceMock<MockDownloadItem>> item_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
};

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       ProductSpecificData_NoSafeBrowsing) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(), safe_browsing::SafeBrowsingState::NO_SAFE_BROWSING);

  auto psd = DownloadWarningHatsProductSpecificData::Create(
      DownloadWarningHatsType::kDownloadBubbleBypass, item_.get());

  // Test the PSD fields added afterwards.
  // This shouldn't do anything because this is a download bubble trigger.
  psd.AddNumPageWarnings(10);
  psd.AddPartialViewInteraction(true);

  // All fields for download bubble are included.
  EXPECT_THAT(psd.bits_data(),
              UnorderedKeysAre(
                  DownloadWarningHatsProductSpecificData::GetBitsDataFields(
                      DownloadWarningHatsType::kDownloadBubbleBypass)));
  EXPECT_THAT(psd.string_data(),
              UnorderedKeysAre(
                  DownloadWarningHatsProductSpecificData::GetStringDataFields(
                      DownloadWarningHatsType::kDownloadBubbleBypass)));

  ExpectDefaultPsd(psd);
  ExpectPlaceholderForSafeBrowsing(psd);
  ExpectPlaceholderForEnhancedSafeBrowsing(psd);

  EXPECT_THAT(
      psd, StringDataMatches(Fields::kSafeBrowsingState, "No Safe Browsing"));
  EXPECT_THAT(psd, StringDataMatches(Fields::kOutcome, HasSubstr("Bypass")));
  EXPECT_THAT(psd, StringDataMatches(Fields::kSurface, HasSubstr("bubble")));

  EXPECT_THAT(psd, BitsDataMatches(Fields::kPartialViewInteraction, true));
  EXPECT_THAT(psd, Not(StringDataMatches(Fields::kNumPageWarnings, _)));
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       ProductSpecificData_StandardSafeBrowsing) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  auto psd = DownloadWarningHatsProductSpecificData::Create(
      DownloadWarningHatsType::kDownloadsPageHeed, item_.get());

  // Test the PSD fields added afterwards.
  psd.AddNumPageWarnings(10);
  // This shouldn't do anything because this is a download page trigger.
  psd.AddPartialViewInteraction(true);

  // All fields for downloads page are included.
  EXPECT_THAT(psd.bits_data(),
              UnorderedKeysAre(
                  DownloadWarningHatsProductSpecificData::GetBitsDataFields(
                      DownloadWarningHatsType::kDownloadsPageHeed)));
  EXPECT_THAT(psd.string_data(),
              UnorderedKeysAre(
                  DownloadWarningHatsProductSpecificData::GetStringDataFields(
                      DownloadWarningHatsType::kDownloadsPageHeed)));

  ExpectDefaultPsd(psd);
  ExpectDefaultPsdForSafeBrowsing(psd);
  ExpectPlaceholderForEnhancedSafeBrowsing(psd);

  EXPECT_THAT(psd, StringDataMatches(Fields::kSafeBrowsingState,
                                     "Standard Protection"));
  EXPECT_THAT(psd, StringDataMatches(Fields::kOutcome, HasSubstr("Heed")));
  EXPECT_THAT(psd, StringDataMatches(Fields::kSurface, HasSubstr("page")));

  EXPECT_THAT(psd, StringDataMatches(Fields::kNumPageWarnings, "10"));
  EXPECT_THAT(psd, Not(BitsDataMatches(Fields::kPartialViewInteraction, _)));
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       ProductSpecificData_EnhancedSafeBrowsing) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);

  auto psd = DownloadWarningHatsProductSpecificData::Create(
      DownloadWarningHatsType::kDownloadBubbleIgnore, item_.get());

  // Test the PSD fields added afterwards.
  // This shouldn't do anything because this is a download bubble trigger.
  psd.AddNumPageWarnings(10);
  psd.AddPartialViewInteraction(true);

  // All fields for download bubble are included.
  EXPECT_THAT(psd.bits_data(),
              UnorderedKeysAre(
                  DownloadWarningHatsProductSpecificData::GetBitsDataFields(
                      DownloadWarningHatsType::kDownloadBubbleIgnore)));
  EXPECT_THAT(psd.string_data(),
              UnorderedKeysAre(
                  DownloadWarningHatsProductSpecificData::GetStringDataFields(
                      DownloadWarningHatsType::kDownloadBubbleIgnore)));

  ExpectDefaultPsd(psd);
  ExpectDefaultPsdForSafeBrowsing(psd);
  ExpectDefaultPsdForEnhancedSafeBrowsing(psd);

  EXPECT_THAT(psd, StringDataMatches(Fields::kSafeBrowsingState,
                                     "Enhanced Protection"));
  EXPECT_THAT(psd, StringDataMatches(Fields::kOutcome, HasSubstr("Ignore")));
  EXPECT_THAT(psd, StringDataMatches(Fields::kSurface, HasSubstr("bubble")));

  EXPECT_THAT(psd, BitsDataMatches(Fields::kPartialViewInteraction, true));
  EXPECT_THAT(psd, Not(StringDataMatches(Fields::kNumPageWarnings, _)));
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       DelayedDownloadWarningHatsLauncher_LaunchesSurvey) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      safe_browsing::kDownloadWarningSurvey,
      {{safe_browsing::kDownloadWarningSurveyType.name,
        "2" /*kDownloadBubbleIgnore*/}});

  DelayedDownloadWarningHatsLauncher launcher{profile_.get(), kIgnoreDelay};
  EXPECT_TRUE(launcher.TryScheduleTask(
      DownloadWarningHatsType::kDownloadBubbleIgnore, item_.get()));
  launcher.RecordBrowserActivity();
  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(kHatsSurveyTriggerDownloadWarningBubbleIgnore, _, _, _, _));
  task_environment_.FastForwardBy(kIgnoreDelay);
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       DelayedDownloadWarningHatsLauncher_DoesntScheduleDuplicateSurvey) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      safe_browsing::kDownloadWarningSurvey,
      {{safe_browsing::kDownloadWarningSurveyType.name,
        "2" /*kDownloadBubbleIgnore*/}});

  DelayedDownloadWarningHatsLauncher launcher{profile_.get(), kIgnoreDelay};
  EXPECT_TRUE(launcher.TryScheduleTask(
      DownloadWarningHatsType::kDownloadBubbleIgnore, item_.get()));
  EXPECT_FALSE(launcher.TryScheduleTask(
      DownloadWarningHatsType::kDownloadBubbleIgnore, item_.get()));
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       DelayedDownloadWarningHatsLauncher_MultipleSurveys) {
  safe_browsing::SetSafeBrowsingState(
      profile_->GetPrefs(),
      safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);

  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      safe_browsing::kDownloadWarningSurvey,
      {{safe_browsing::kDownloadWarningSurveyType.name,
        "2" /*kDownloadBubbleIgnore*/}});

  std::unique_ptr<NiceMock<MockDownloadItem>> other_item =
      SetUpMockDownloadItem();
  ON_CALL(*other_item, GetFileNameToReportUser())
      .WillByDefault(
          Return(base::FilePath(FILE_PATH_LITERAL("other_file.pdf"))));

  DelayedDownloadWarningHatsLauncher launcher{profile_.get(), kIgnoreDelay};
  EXPECT_TRUE(launcher.TryScheduleTask(
      DownloadWarningHatsType::kDownloadBubbleIgnore, item_.get()));
  launcher.RecordBrowserActivity();
  task_environment_.FastForwardBy(kIgnoreDelay / 2);
  EXPECT_TRUE(launcher.TryScheduleTask(
      DownloadWarningHatsType::kDownloadBubbleIgnore, other_item.get()));
  {
    EXPECT_CALL(
        *mock_hats_service_,
        LaunchSurvey(
            kHatsSurveyTriggerDownloadWarningBubbleIgnore, _, _, _,
            Contains(Pair(Fields::kFilename, HasSubstr("my_file.pdf")))));
    task_environment_.FastForwardBy(kIgnoreDelay / 2);
  }
  launcher.RecordBrowserActivity();
  {
    EXPECT_CALL(
        *mock_hats_service_,
        LaunchSurvey(
            kHatsSurveyTriggerDownloadWarningBubbleIgnore, _, _, _,
            Contains(Pair(Fields::kFilename, HasSubstr("other_file.pdf")))));
    task_environment_.FastForwardBy(kIgnoreDelay / 2);
  }
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       DelayedDownloadWarningHatsLauncher_WithholdsSurveyIfNoUserActivity) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      safe_browsing::kDownloadWarningSurvey,
      {{safe_browsing::kDownloadWarningSurveyType.name,
        "2" /*kDownloadBubbleIgnore*/}});

  DelayedDownloadWarningHatsLauncher launcher{profile_.get(), kIgnoreDelay};
  launcher.TryScheduleTask(DownloadWarningHatsType::kDownloadBubbleIgnore,
                           item_.get());
  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(kHatsSurveyTriggerDownloadWarningBubbleIgnore, _, _, _, _))
      .Times(0);
  task_environment_.FastForwardBy(2 * kIgnoreDelay);
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       DelayedDownloadWarningHatsLauncher_DeletesTaskWhenItemDeleted) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      safe_browsing::kDownloadWarningSurvey,
      {{safe_browsing::kDownloadWarningSurveyType.name,
        "2" /*kDownloadBubbleIgnore*/}});

  DelayedDownloadWarningHatsLauncher launcher{profile_.get(), kIgnoreDelay};
  EXPECT_TRUE(launcher.TryScheduleTask(
      DownloadWarningHatsType::kDownloadBubbleIgnore, item_.get()));
  item_.reset();

  launcher.RecordBrowserActivity();
  EXPECT_CALL(
      *mock_hats_service_,
      LaunchSurvey(kHatsSurveyTriggerDownloadWarningBubbleIgnore, _, _, _, _))
      .Times(0);
  task_environment_.FastForwardBy(kIgnoreDelay);

  item_ = SetUpMockDownloadItem();
  // Trying again works because the older task was deleted.
  EXPECT_TRUE(launcher.TryScheduleTask(
      DownloadWarningHatsType::kDownloadBubbleIgnore, item_.get()));
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       MaybeGetDownloadWarningHatsTrigger_FeatureDisabled) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(safe_browsing::kDownloadWarningSurvey);

  for (DownloadWarningHatsType type :
       {DownloadWarningHatsType::kDownloadBubbleBypass,
        DownloadWarningHatsType::kDownloadBubbleHeed,
        DownloadWarningHatsType::kDownloadBubbleIgnore,
        DownloadWarningHatsType::kDownloadsPageBypass,
        DownloadWarningHatsType::kDownloadsPageHeed,
        DownloadWarningHatsType::kDownloadsPageIgnore}) {
    EXPECT_FALSE(MaybeGetDownloadWarningHatsTrigger(type));
  }
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       MaybeGetDownloadWarningHatsTrigger_ParamOutOfRange) {
  for (DownloadWarningHatsType type :
       {DownloadWarningHatsType::kDownloadBubbleBypass,
        DownloadWarningHatsType::kDownloadBubbleHeed,
        DownloadWarningHatsType::kDownloadBubbleIgnore,
        DownloadWarningHatsType::kDownloadsPageBypass,
        DownloadWarningHatsType::kDownloadsPageHeed,
        DownloadWarningHatsType::kDownloadsPageIgnore}) {
    for (const std::string& param_value : {"", "-1", "6"}) {
      base::test::ScopedFeatureList features;
      features.InitAndEnableFeatureWithParameters(
          safe_browsing::kDownloadWarningSurvey,
          {{safe_browsing::kDownloadWarningSurveyType.name, param_value}});

      EXPECT_FALSE(MaybeGetDownloadWarningHatsTrigger(type));
    }
  }
}

TEST_F(DownloadWarningDesktopHatsUtilsTest,
       MaybeGetDownloadWarningHatsTrigger_OnlyReturnsTriggerIfEligible) {
  for (DownloadWarningHatsType type :
       {DownloadWarningHatsType::kDownloadBubbleBypass,
        DownloadWarningHatsType::kDownloadBubbleHeed,
        DownloadWarningHatsType::kDownloadBubbleIgnore,
        DownloadWarningHatsType::kDownloadsPageBypass,
        DownloadWarningHatsType::kDownloadsPageHeed,
        DownloadWarningHatsType::kDownloadsPageIgnore}) {
    for (int param_value = 0; param_value < 6; ++param_value) {
      std::string param_value_string = base::NumberToString(param_value);
      base::test::ScopedFeatureList features;
      features.InitAndEnableFeatureWithParameters(
          safe_browsing::kDownloadWarningSurvey,
          {{safe_browsing::kDownloadWarningSurveyType.name,
            param_value_string}});

      bool eligible = param_value == static_cast<int>(type);
      EXPECT_EQ(MaybeGetDownloadWarningHatsTrigger(type).has_value(), eligible);
    }
  }
}

}  // namespace
