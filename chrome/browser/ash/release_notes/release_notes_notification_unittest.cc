// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/release_notes/release_notes_notification.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

class ReleaseNotesNotificationTest : public BrowserWithTestWindowTest {
 public:
  ReleaseNotesNotificationTest() {}

  ReleaseNotesNotificationTest(const ReleaseNotesNotificationTest&) = delete;
  ReleaseNotesNotificationTest& operator=(const ReleaseNotesNotificationTest&) =
      delete;

  ~ReleaseNotesNotificationTest() override = default;

  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    return profile_manager()->CreateTestingProfile("googler@google.com");
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(nullptr);
    tester_->SetNotificationAddedClosure(
        base::BindRepeating(&ReleaseNotesNotificationTest::OnNotificationAdded,
                            base::Unretained(this)));
    release_notes_notification_ =
        std::make_unique<ReleaseNotesNotification>(profile());
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kReleaseNotesNotificationAllChannels},
        /*disabled_features=*/{});
  }

  void TearDown() override {
    release_notes_notification_.reset();
    tester_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  void OnNotificationAdded() { notification_count_++; }

 protected:
  bool HasReleaseNotesNotification() {
    return tester_->GetNotification("show_release_notes_notification")
        .has_value();
  }

  message_center::Notification GetReleaseNotesNotification() {
    return tester_->GetNotification("show_release_notes_notification").value();
  }

  int notification_count_ = 0;
  std::unique_ptr<ReleaseNotesNotification> release_notes_notification_;

 private:
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ReleaseNotesNotificationTest, DoNotShowReleaseNotesNotification) {
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile());
  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone,
      version_info::GetVersion().components()[0]);
  release_notes_notification_->MaybeShowReleaseNotes();
  EXPECT_EQ(false, HasReleaseNotesNotification());
  EXPECT_EQ(0, notification_count_);
}

TEST_F(ReleaseNotesNotificationTest, ShowReleaseNotesNotification) {
  std::unique_ptr<ReleaseNotesStorage> release_notes_storage =
      std::make_unique<ReleaseNotesStorage>(profile());
  profile()->GetPrefs()->SetInteger(
      prefs::kHelpAppNotificationLastShownMilestone, 20);
  release_notes_notification_->MaybeShowReleaseNotes();
  EXPECT_EQ(true, HasReleaseNotesNotification());
  EXPECT_EQ(ui::SubstituteChromeOSDeviceType(
                IDS_RELEASE_NOTES_DEVICE_SPECIFIC_NOTIFICATION_TITLE),
            GetReleaseNotesNotification().title());
  EXPECT_EQ("Get highlights from the latest update",
            base::UTF16ToASCII(GetReleaseNotesNotification().message()));
  EXPECT_EQ(1, notification_count_);
}

}  // namespace ash
