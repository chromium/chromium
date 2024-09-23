// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sanitize/chrome_sanitize_ui_delegate.h"

#include "chrome/browser/profile_resetter/fake_profile_resetter.h"
#include "chrome/browser/ui/webui/ash/settings/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class FakeChromeSanitizeUIDelegate : public ChromeSanitizeUIDelegate {
 public:
  FakeChromeSanitizeUIDelegate(TestingProfile* profile,
                               content::TestWebUI* web_ui)
      : ChromeSanitizeUIDelegate(web_ui), resetter_(profile) {}

  FakeProfileResetter* GetResetter() override { return &resetter_; }
  void RestartChrome() override {}

 private:
  FakeProfileResetter resetter_;
};

class ChromeSanitizeUIDelegateTest : public testing::Test {
 public:
  ChromeSanitizeUIDelegateTest() {
    auto web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&profile_));
    web_ui_.set_web_contents(web_contents_.get());
    delegate_ =
        std::make_unique<FakeChromeSanitizeUIDelegate>(&profile_, &web_ui_);
  }

  void PerformSanitizeSettings() { delegate_->PerformSanitizeSettings(); }

  PrefService* GetPrefs() { return profile_.GetPrefs(); }

 private:
  // task_environment_ should be declared before profile_ to protect against
  // thread crashes.
  content::BrowserTaskEnvironment task_environment_;
  content::TestWebUI web_ui_;
  TestingProfile profile_;
  std::unique_ptr<FakeChromeSanitizeUIDelegate> delegate_;
};

TEST_F(ChromeSanitizeUIDelegateTest, SanitizeTest) {
  EXPECT_EQ(false,
            GetPrefs()->GetBoolean(ash::settings::prefs::kSanitizeCompleted));
  PerformSanitizeSettings();
  EXPECT_EQ(true,
            GetPrefs()->GetBoolean(ash::settings::prefs::kSanitizeCompleted));
}

}  // namespace

}  // namespace ash
