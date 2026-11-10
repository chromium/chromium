// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_CHROME_LINUX_UI_GETTER_H_
#define CHROME_BROWSER_THEMES_CHROME_LINUX_UI_GETTER_H_

#include "ui/linux/linux_ui_getter.h"

class Profile;

// Extends `ui::LinuxUiGetter` with `Profile`-based theme lookup for Chrome.
// Prefer `ThemeServiceAuraLinux::GetLinuxUiThemeForProfile()` over calling this
// class directly.
class ChromeLinuxUiGetter : public ui::LinuxUiGetter {
 public:
  ChromeLinuxUiGetter();
  ChromeLinuxUiGetter(const ChromeLinuxUiGetter&) = delete;
  ChromeLinuxUiGetter& operator=(const ChromeLinuxUiGetter&) = delete;
  ~ChromeLinuxUiGetter() override;

  virtual ui::LinuxUiTheme* GetForProfile(const Profile* profile) = 0;

  static ChromeLinuxUiGetter* instance() {
    return ui::LinuxUiGetter::instance() == instance_ ? instance_ : nullptr;
  }

 private:
  static ChromeLinuxUiGetter* instance_;
};

#endif  // CHROME_BROWSER_THEMES_CHROME_LINUX_UI_GETTER_H_
