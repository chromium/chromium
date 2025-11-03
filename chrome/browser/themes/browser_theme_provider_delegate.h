// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_BROWSER_THEME_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_THEMES_BROWSER_THEME_PROVIDER_DELEGATE_H_

class CustomThemeSupplier;

// Provides Browser-specific theme information.
class BrowserThemeProviderDelegate {
 public:
  virtual ~BrowserThemeProviderDelegate() = default;
  virtual CustomThemeSupplier* GetThemeSupplier() const = 0;
  virtual bool ShouldUseCustomFrame() const = 0;
};

#endif  // CHROME_BROWSER_THEMES_BROWSER_THEME_PROVIDER_DELEGATE_H_
