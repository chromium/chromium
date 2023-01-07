// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_UI_DELEGATE_H_

#include "ash/webui/help_app_ui/help_app_ui_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash {

/**
 * Implementation of the HelpAppUiDelegate interface. Provides the help app
 * code in chromeos/ with functions that only exist in chrome/.
 */
class ChromeHelpAppUIDelegate : public HelpAppUIDelegate {
 public:
  explicit ChromeHelpAppUIDelegate(content::WebUI* web_ui);

  ChromeHelpAppUIDelegate(const ChromeHelpAppUIDelegate&) = delete;
  ChromeHelpAppUIDelegate& operator=(const ChromeHelpAppUIDelegate&) = delete;

  // HelpAppUIDelegate:
  absl::optional<std::string> OpenFeedbackDialog() override;
  void ShowParentalControls() override;
  PrefService* GetLocalState() override;
  void MaybeShowDiscoverNotification() override;
  void MaybeShowReleaseNotesNotification() override;

 private:
  content::WebUI* web_ui_;  // Owns |this|.
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_HELP_APP_HELP_APP_UI_DELEGATE_H_
