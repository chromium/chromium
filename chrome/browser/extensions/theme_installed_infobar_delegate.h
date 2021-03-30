// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "extensions/common/extension_id.h"
#include "third_party/skia/include/core/SkColor.h"

class InfoBarService;

// When a user installs a theme, we display it immediately, but provide an
// infobar allowing them to cancel.
class ThemeInstalledInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public ThemeServiceObserver {
 public:
  // Creates a theme installed infobar and delegate and adds the infobar to
  // |infobar_service|, replacing any previous theme infobar.
  static void Create(
      InfoBarService* infobar_service,
      ThemeService* theme_service,
      const std::string& theme_name,
      const std::string& theme_id,
      std::unique_ptr<ThemeService::ThemeReinstaller> prev_theme_reinstaller);

 private:
  ThemeInstalledInfoBarDelegate(
      ThemeService* theme_service,
      const std::string& theme_name,
      const std::string& theme_id,
      std::unique_ptr<ThemeService::ThemeReinstaller> prev_theme_reinstaller);
  ~ThemeInstalledInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  ThemeInstalledInfoBarDelegate* AsThemePreviewInfobarDelegate() override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Cancel() override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  ThemeService* theme_service_;

  // Name of theme that's just been installed.
  std::string theme_name_;

  // ID of theme that's just been installed.
  std::string theme_id_;

  // Used to undo theme install.
  std::unique_ptr<ThemeService::ThemeReinstaller> prev_theme_reinstaller_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
