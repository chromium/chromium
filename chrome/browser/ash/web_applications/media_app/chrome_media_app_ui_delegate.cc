// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/media_app/chrome_media_app_ui_delegate.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/media_app_ui/url_constants.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "url/gurl.h"

ChromeMediaAppUIDelegate::ChromeMediaAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

absl::optional<std::string> ChromeMediaAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  constexpr char kMediaAppFeedbackCategoryTag[] = "FromMediaApp";

  // TODO(crbug/1045222): Additional strings are blank right now while we decide
  // on the language and relevant information we want feedback to include.
  // Note that category_tag is the name of the listnr bucket we want our
  // reports to end up in.
  chrome::ShowFeedbackPage(GURL(ash::kChromeUIMediaAppURL), profile,
                           chrome::kFeedbackSourceMediaApp,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kMediaAppFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);

  // TODO(crbug/1048368): Showing the feedback dialog can fail, communicate this
  // back to the client with an error string. For now assume dialog opened.
  return absl::nullopt;
}

void ChromeMediaAppUIDelegate::ToggleBrowserFullscreenMode() {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui_->GetWebContents());
  if (browser) {
    chrome::ToggleFullscreenMode(browser);
  }
}
