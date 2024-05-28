// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "extensions/common/extension_id.h"
#include "third_party/skia/include/core/SkColor.h"

namespace infobars {
class ContentInfoBarManager;
}

class Profile;

// When a user installs a theme, we display it immediately, but provide an
// infobar allowing them to cancel.
class ThemeInstalledInfoBarDelegate : public ConfirmInfoBarDelegate,
                                      public ThemeServiceObserver {
 public:
  // This method does nothing if there is no tabbed browser open for `profile`.
  static void CreateForLastActiveTab(
      Profile* profile,
      const std::string& theme_name,
      const std::string& theme_id,
      std::unique_ptr<ThemeService::ThemeReinstaller> prev_theme_reinstaller);

 private:
  friend class InfoBarUiTest;

  // Creates a theme installed infobar and delegate and adds the infobar to
  // |infobar_manager|, replacing any previous theme infobar.
  static void Create(
      infobars::ContentInfoBarManager* infobar_manager,
      ThemeService* theme_service,
      const std::string& theme_name,
      const std::string& theme_id,
      std::unique_ptr<ThemeService::ThemeReinstaller> prev_theme_reinstaller);

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

  raw_ptr<ThemeService> theme_service_;

  // Name of theme that's just been installed.
  std::string theme_name_;

  // ID of theme that's just been installed.
  std::string theme_id_;

  // Used to undo theme install.
  std::unique_ptr<ThemeService::ThemeReinstaller> prev_theme_reinstaller_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_THEME_INSTALLED_INFOBAR_DELEGATE_H_
