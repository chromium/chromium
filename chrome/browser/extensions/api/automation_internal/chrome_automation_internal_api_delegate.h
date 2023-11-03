// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_CHROME_AUTOMATION_INTERNAL_API_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_CHROME_AUTOMATION_INTERNAL_API_DELEGATE_H_

#include "extensions/browser/api/automation_internal/automation_internal_api_delegate.h"

namespace extensions {

class AutomationEventRouterInterface;

// A delegate for chrome specific automation api logic.
class ChromeAutomationInternalApiDelegate
    : public AutomationInternalApiDelegate {
 public:
  ChromeAutomationInternalApiDelegate();
  ChromeAutomationInternalApiDelegate(
      const ChromeAutomationInternalApiDelegate&) = delete;
  ChromeAutomationInternalApiDelegate& operator=(
      const ChromeAutomationInternalApiDelegate&) = delete;
  ~ChromeAutomationInternalApiDelegate() override;

  bool CanRequestAutomation(const Extension* extension,
                            const AutomationInfo* automation_info,
                            content::WebContents* contents) override;
  bool EnableTree(const ui::AXTreeID& tree_id) override;
  void EnableDesktop() override;
  ui::AXTreeID GetAXTreeID() override;
  void SetAutomationEventRouterInterface(
      AutomationEventRouterInterface* router) override;
  content::BrowserContext* GetActiveUserContext() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_CHROME_AUTOMATION_INTERNAL_API_DELEGATE_H_
