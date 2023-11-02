// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_butter_bar.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/lacros_prefs.h"
#include "chrome/browser/lacros/lacros_startup_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"

LacrosButterBar::LacrosButterBar() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // We do not show a butter bar for kiosk, tests or automation. By early
  // returning here, this class will do nothing since no observer has been
  // added.
  if (command_line->HasSwitch(switches::kKioskMode) ||
      command_line->HasSwitch(switches::kTestType) ||
      command_line->HasSwitch(switches::kEnableAutomation)) {
    return;
  }

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  // We show the banner if it's never shown before.
  bool should_show_banner =
      !local_state->GetBoolean(lacros_prefs::kShowedExperimentalBannerPref);

  // Always show the banner for now, until privacy concerns are addressed.
  // https://crbug.com/1259732.
  should_show_banner |= true;
  if (!should_show_banner)
    return;

  // Check existing browsers that were created before this class was
  // constructed.
  for (Browser* browser : *BrowserList::GetInstance()) {
    OnBrowserAdded(browser);
  }

  // Add an observation for future Browsers. Note that it's possible that the
  // earlier logic that enumates all Browsers already presented a butter bar, in
  // which case there's no reason to add an observaton here.
  if (!presented_butter_bar_) {
    observing_browser_list_ = true;
    BrowserList::AddObserver(this);
  }
}

LacrosButterBar::~LacrosButterBar() {
  if (observing_browser_list_) {
    BrowserList::RemoveObserver(this);
    observing_browser_list_ = false;
  }
}

void LacrosButterBar::OnBrowserAdded(Browser* browser) {
  if (presented_butter_bar_)
    return;

  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!contents)
    return;

  ShowBanner(contents);
}

void LacrosButterBar::ShowBanner(content::WebContents* web_contents) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);

  LacrosStartupInfoBarDelegate::Create(infobar_manager);

  PrefService* local_state = g_browser_process->local_state();
  local_state->SetBoolean(lacros_prefs::kShowedExperimentalBannerPref, true);

  presented_butter_bar_ = true;
  if (observing_browser_list_) {
    BrowserList::RemoveObserver(this);
    observing_browser_list_ = false;
  }
}
