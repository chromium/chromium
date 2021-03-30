// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/chrome_media_app_ui_delegate.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/channel_info.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui_data_source.h"
#include "url/gurl.h"

ChromeMediaAppUIDelegate::ChromeMediaAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

base::Optional<std::string> ChromeMediaAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  constexpr char kMediaAppFeedbackCategoryTag[] = "FromMediaApp";

  // TODO(crbug/1045222): Additional strings are blank right now while we decide
  // on the language and relevant information we want feedback to include.
  // Note that category_tag is the name of the listnr bucket we want our
  // reports to end up in.
  chrome::ShowFeedbackPage(GURL(chromeos::kChromeUIMediaAppURL), profile,
                           chrome::kFeedbackSourceMediaApp,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           kMediaAppFeedbackCategoryTag /* category_tag */,
                           std::string() /* extra_diagnostics */);

  // TODO(crbug/1048368): Showing the feedback dialog can fail, communicate this
  // back to the client with an error string. For now assume dialog opened.
  return base::nullopt;
}

void ChromeMediaAppUIDelegate::PopulateLoadTimeData(
    content::WebUIDataSource* source) {
  source->AddString("appLocale", g_browser_process->GetApplicationLocale());
  source->AddBoolean(
      "imageAnnotation",
      base::FeatureList::IsEnabled(chromeos::features::kMediaAppAnnotation));
  source->AddBoolean(
      "displayExif",
      base::FeatureList::IsEnabled(chromeos::features::kMediaAppDisplayExif));
  source->AddBoolean("pdfInInk", base::FeatureList::IsEnabled(
                                     chromeos::features::kMediaAppPdfInInk));
  version_info::Channel channel = chrome::GetChannel();
  source->AddBoolean("flagsMenu", channel != version_info::Channel::BETA &&
                                      channel != version_info::Channel::STABLE);
  source->AddBoolean("isDevChannel", channel == version_info::Channel::DEV);
  source->AddBoolean(
      "videoControls",
      base::FeatureList::IsEnabled(chromeos::features::kMediaAppVideoControls));
}
