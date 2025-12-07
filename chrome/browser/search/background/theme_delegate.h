// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_THEME_DELEGATE_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_THEME_DELEGATE_H_

#include "base/observer_list_types.h"
#include "third_party/skia/include/core/SkColor.h"

// Helper interface for the NtpCustomBackgroundService to query theme state and
// provide updates.
// TODO(http://crbug.com/341787825): Remove delegate.
class ThemeDelegate : public base::CheckedObserver {
 public:
  ~ThemeDelegate() override;

  // Called when the background color should be updated.
  virtual void OnBackgroundColorExtracted(SkColor color) = 0;

  // Called when the service is shutting down.
  virtual void OnNtpCustomBackgroundServiceShuttingDown() = 0;

  // Whether the theme is provided by an extension.
  virtual bool UsingExtensionTheme() const = 0;

 protected:
  ThemeDelegate();
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_THEME_DELEGATE_H_
