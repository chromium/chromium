// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_util_win.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/default_settings_fetcher.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

void TryToShowSettingsResetPrompt(
    std::unique_ptr<SettingsResetPromptModel> model,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings) {
  DCHECK(model);
  DCHECK(default_settings);

  // Ensure that there is at least one non-incognito browser open for the
  // profile before attempting to show the dialog.
  Browser* browser = chrome::FindTabbedBrowser(
      model->profile(), /*match_original_profiles=*/false);
  if (!browser)
    return;

  // First, show the browser window, and only then show the dialog so that the
  // dialog will have focus.
  if (browser->window()->IsMinimized())
    browser->window()->Restore();
  browser->window()->Show();

  // The |SettingsResetPromptController| object will delete itself after the
  // reset prompt dialog has been closed.
  chrome::ShowSettingsResetPrompt(
      browser, new SettingsResetPromptController(std::move(model),
                                                 std::move(default_settings)));
}

// Will display the settings reset prompt if required and if there is at least
// one non-incognito browser available for the corresponding profile.
void MaybeShowSettingsResetPrompt(
    std::unique_ptr<SettingsResetPromptConfig> config) {
  DCHECK(config);

  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  // Get the original profile in case the last active browser was incognito. We
  // ensure that there is at least one non-incognito browser open before
  // displaying the dialog.
  Profile* profile = browser->profile()->GetOriginalProfile();

  auto model = std::make_unique<SettingsResetPromptModel>(
      profile, std::move(config), std::make_unique<ProfileResetter>(profile));

  model->ReportUmaMetrics();

  if (!model->ShouldPromptForReset())
    return;

  DefaultSettingsFetcher::FetchDefaultSettings(
      base::Bind(&TryToShowSettingsResetPrompt, base::Passed(&model)));
}

class SettingsResetPromptDelegateImpl : public SettingsResetPromptDelegate {
 public:
  SettingsResetPromptDelegateImpl();
  ~SettingsResetPromptDelegateImpl() override;

  void ShowSettingsResetPromptWithDelay() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptDelegateImpl);
};

SettingsResetPromptDelegateImpl::SettingsResetPromptDelegateImpl() = default;

SettingsResetPromptDelegateImpl::~SettingsResetPromptDelegateImpl() = default;

void SettingsResetPromptDelegateImpl::ShowSettingsResetPromptWithDelay() const {
  std::unique_ptr<SettingsResetPromptConfig> config =
      SettingsResetPromptConfig::Create();
  if (!config)
    return;

  base::TimeDelta delay = config->delay_before_prompt();
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(MaybeShowSettingsResetPrompt, base::Passed(&config)),
      delay);
}

SettingsResetPromptDelegate* g_settings_reset_prompt_delegate = nullptr;

}  // namespace

void MaybeShowSettingsResetPromptWithDelay() {
  if (g_settings_reset_prompt_delegate) {
    g_settings_reset_prompt_delegate->ShowSettingsResetPromptWithDelay();
  } else {
    SettingsResetPromptDelegateImpl().ShowSettingsResetPromptWithDelay();
  }
}

SettingsResetPromptDelegate::SettingsResetPromptDelegate() = default;

SettingsResetPromptDelegate::~SettingsResetPromptDelegate() = default;

void SetSettingsResetPromptDelegate(SettingsResetPromptDelegate* delegate) {
  g_settings_reset_prompt_delegate = delegate;
}

}  // namespace safe_browsing
