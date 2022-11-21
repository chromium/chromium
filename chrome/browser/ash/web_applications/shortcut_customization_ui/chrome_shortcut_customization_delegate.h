// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_SHORTCUT_CUSTOMIZATION_UI_CHROME_SHORTCUT_CUSTOMIZATION_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_SHORTCUT_CUSTOMIZATION_UI_CHROME_SHORTCUT_CUSTOMIZATION_DELEGATE_H_

#include "ash/webui/shortcut_customization_ui/backend/shortcut_customization_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"

/**
 * Implementation of the ShortcutCustomizationDelegate interface. Provides the
 * shortcut customization app code in ash/ with functions that only exist in
 * chrome/.
 */
class ChromeShortcutCustomizationDelegate
    : public ash::shortcut_ui::ShortcutCustomizationDelegate {
 public:
  explicit ChromeShortcutCustomizationDelegate(content::WebUI* web_ui);

  ChromeShortcutCustomizationDelegate(
      const ChromeShortcutCustomizationDelegate&) = delete;
  ChromeShortcutCustomizationDelegate& operator=(
      const ChromeShortcutCustomizationDelegate&) = delete;
  ~ChromeShortcutCustomizationDelegate() override;

  // Get the pref service.
  PrefService* GetPrefService() override;

 private:
  content::WebUI* web_ui_;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_SHORTCUT_CUSTOMIZATION_UI_CHROME_SHORTCUT_CUSTOMIZATION_DELEGATE_H_
