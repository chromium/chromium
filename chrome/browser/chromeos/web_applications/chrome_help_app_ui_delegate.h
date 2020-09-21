// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_HELP_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_HELP_APP_UI_DELEGATE_H_

#include "base/optional.h"
#include "chromeos/components/help_app_ui/help_app_ui_delegate.h"

namespace content {
class WebUI;
}

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
  base::Optional<std::string> OpenFeedbackDialog() override;
  void PopulateLoadTimeData(content::WebUIDataSource* source) override;
  void ShowParentalControls() override;
  PrefService* GetLocalState() override;

 private:
  content::WebUI* web_ui_;  // Owns |this|.
};

#endif  // CHROME_BROWSER_CHROMEOS_WEB_APPLICATIONS_CHROME_HELP_APP_UI_DELEGATE_H_
