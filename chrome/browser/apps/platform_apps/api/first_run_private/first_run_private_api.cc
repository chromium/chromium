// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/first_run_private/first_run_private_api.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chrome_apps {
namespace api {

ExtensionFunction::ResponseAction
FirstRunPrivateGetLocalizedStringsFunction::Run() {
  UMA_HISTOGRAM_COUNTS_1M("CrosFirstRun.DialogShown", 1);
  std::unique_ptr<base::DictionaryValue> localized_strings(
      new base::DictionaryValue());
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!user->GetGivenName().empty()) {
    localized_strings->SetString(
        "greetingHeader",
        l10n_util::GetStringFUTF16(IDS_FIRST_RUN_GREETING_STEP_HEADER,
                                   user->GetGivenName()));
  } else {
    localized_strings->SetString(
        "greetingHeader",
        l10n_util::GetStringUTF16(IDS_FIRST_RUN_GREETING_STEP_HEADER_GENERAL));
  }
  base::string16 product_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  localized_strings->SetString(
      "greetingText1", l10n_util::GetStringFUTF16(
                           IDS_FIRST_RUN_GREETING_STEP_TEXT_1, product_name));
  localized_strings->SetString(
      "greetingText2", l10n_util::GetStringFUTF16(
                           IDS_FIRST_RUN_GREETING_STEP_TEXT_2, product_name));
  localized_strings->SetString(
      "greetingButton",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_GREETING_STEP_BUTTON));
  localized_strings->SetString("closeButton",
                               l10n_util::GetStringUTF16(IDS_CLOSE));
  localized_strings->SetString(
      "accessibleTitle",
      l10n_util::GetStringUTF16(IDS_FIRST_RUN_ACCESSIBLE_TITLE));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings.get());

  return RespondNow(OneArgument(std::move(localized_strings)));
}

ExtensionFunction::ResponseAction FirstRunPrivateLaunchTutorialFunction::Run() {
  chromeos::first_run::LaunchTutorial();
  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace chrome_apps
