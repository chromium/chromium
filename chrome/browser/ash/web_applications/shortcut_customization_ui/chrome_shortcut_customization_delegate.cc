// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/shortcut_customization_ui/chrome_shortcut_customization_delegate.h"
#include "chrome/browser/profiles/profile.h"

ChromeShortcutCustomizationDelegate::ChromeShortcutCustomizationDelegate(
    content::WebUI* web_ui)
    : web_ui_(web_ui) {}

ChromeShortcutCustomizationDelegate::~ChromeShortcutCustomizationDelegate() =
    default;

PrefService* ChromeShortcutCustomizationDelegate::GetPrefService() {
  return Profile::FromWebUI(web_ui_)->GetPrefs();
}
