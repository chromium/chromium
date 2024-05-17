// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/audio/cras_audio_handler_delegate_impl.h"

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "url/gurl.h"

namespace ash {

namespace {

class TestSettingsWindowManager : public chrome::SettingsWindowManager {
 public:
  void ShowChromePageForProfile(Profile* profile,
                                const GURL& gurl,
                                int64_t display_id,
                                apps::LaunchCallback callback) override {
    last_url_ = gurl;
    if (callback) {
      std::move(callback).Run(apps::LaunchResult(apps::State::kSuccess));
    }
  }
  const GURL& last_url() { return last_url_; }

 private:
  GURL last_url_;
};

// Use BrowserWithTestWindowTest because it sets up ash::Shell, ash::SystemTray,
// ProfileManager, etc.
class CrasAudioHandlerDelegateImplTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    cras_audio_handler_delegate_impl_ =
        std::make_unique<CrasAudioHandlerDelegateImpl>();
    settings_window_manager_ = std::make_unique<TestSettingsWindowManager>();
    chrome::SettingsWindowManager::SetInstanceForTesting(
        settings_window_manager_.get());
  }

  void TearDown() override {
    chrome::SettingsWindowManager::SetInstanceForTesting(nullptr);
    settings_window_manager_.reset();
    cras_audio_handler_delegate_impl_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  std::unique_ptr<CrasAudioHandlerDelegateImpl>
      cras_audio_handler_delegate_impl_;
  std::unique_ptr<TestSettingsWindowManager> settings_window_manager_;
};

TEST_F(CrasAudioHandlerDelegateImplTest, OpenSettingsAudioPage) {
  cras_audio_handler_delegate_impl_->OpenSettingsAudioPage();
  EXPECT_EQ(
      settings_window_manager_->last_url(),
      chrome::GetOSSettingsUrl(chromeos::settings::mojom::kAudioSubpagePath));
}

}  // namespace

}  // namespace ash
