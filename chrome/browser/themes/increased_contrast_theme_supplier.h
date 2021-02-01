// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_INCREASED_CONTRAST_THEME_SUPPLIER_H_
#define CHROME_BROWSER_THEMES_INCREASED_CONTRAST_THEME_SUPPLIER_H_

#include "chrome/browser/themes/custom_theme_supplier.h"
#include "ui/native_theme/native_theme_observer.h"

namespace ui {
class NativeTheme;
}  // namespace ui

// A theme supplier that maximizes the contrast between UI elements and
// especially the visual prominence of key UI elements (omnibox, active vs
// inactive tab distinction).
class IncreasedContrastThemeSupplier : public CustomThemeSupplier,
                                       public ui::NativeThemeObserver {
 public:
  explicit IncreasedContrastThemeSupplier(ui::NativeTheme* theme);

  bool GetColor(int id, SkColor* color) const override;
  bool CanUseIncognitoColors() const override;

 protected:
  ~IncreasedContrastThemeSupplier() override;

 private:
  void OnNativeThemeUpdated(ui::NativeTheme* native_theme) override;

  ui::NativeTheme* native_theme_;
  bool is_dark_mode_;

  DISALLOW_COPY_AND_ASSIGN(IncreasedContrastThemeSupplier);
};

#endif  // CHROME_BROWSER_THEMES_INCREASED_CONTRAST_THEME_SUPPLIER_H_
