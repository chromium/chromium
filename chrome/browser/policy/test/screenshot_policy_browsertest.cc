// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "base/files/file_enumerator.h"
#include "base/run_loop.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"

namespace policy {

namespace {

constexpr char kScreenCaptureNotificationId[] = "capture_mode_notification";

int CountScreenshots() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(ProfileManager::GetActiveUserProfile());
  base::FileEnumerator enumerator(
      download_prefs->GetDefaultDownloadDirectoryForProfile(), false,
      base::FileEnumerator::FILES, "Screenshot*");
  int count = 0;
  while (!enumerator.Next().empty())
    count++;
  return count;
}

class CaptureNotificationWaiter : public message_center::MessageCenterObserver {
 public:
  CaptureNotificationWaiter() {
    message_center::MessageCenter::Get()->AddObserver(this);
  }
  ~CaptureNotificationWaiter() override {
    message_center::MessageCenter::Get()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& notification_id) override {
    if (notification_id == kScreenCaptureNotificationId)
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

class ScreenshotPolicyTest : public PolicyTest {
 public:
  ScreenshotPolicyTest() = default;
  ~ScreenshotPolicyTest() override = default;

  void SetScreenshotPolicy(bool enabled) {
    PolicyMap policies;
    policies.Set(key::kDisableScreenshots, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(!enabled),
                 nullptr);
    UpdateProviderPolicy(policies);
  }

  void TestScreenshotFile(bool enabled) {
    CaptureNotificationWaiter waiter;
    SetScreenshotPolicy(enabled);
    ash::CaptureScreenshotsOfAllDisplays();
    waiter.Wait();
  }
};

IN_PROC_BROWSER_TEST_F(ScreenshotPolicyTest, DisableScreenshotsFile) {
  const int screenshot_count = CountScreenshots();

  // Make sure screenshots are counted correctly.
  TestScreenshotFile(true);
  ASSERT_EQ(CountScreenshots(), screenshot_count + 1);

  // Check if trying to take a screenshot fails when disabled by policy.
  TestScreenshotFile(false);
  ASSERT_EQ(CountScreenshots(), screenshot_count + 1);
}

}  // namespace policy
