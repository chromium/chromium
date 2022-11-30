// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_SCREEN_ORIENTATION_DELEGATE_LACROS_H_
#define CHROME_BROWSER_LACROS_SCREEN_ORIENTATION_DELEGATE_LACROS_H_

#include "content/public/browser/screen_orientation_delegate.h"

// Chrome OS implementation for screen orientation JS api.
class ScreenOrientationDelegateLacros
    : public content::ScreenOrientationDelegate {
 public:
  ScreenOrientationDelegateLacros();
  ScreenOrientationDelegateLacros(const ScreenOrientationDelegateLacros&) =
      delete;
  ScreenOrientationDelegateLacros& operator=(
      const ScreenOrientationDelegateLacros&) = delete;
  ~ScreenOrientationDelegateLacros() override;

  // content::ScreenOrientationDelegate:
  bool FullScreenRequired(content::WebContents* web_contents) override;
  void Lock(content::WebContents* web_contents,
            device::mojom::ScreenOrientationLockType lock_orientation) override;
  bool ScreenOrientationProviderSupported(
      content::WebContents* web_contents) override;
  void Unlock(content::WebContents* web_contents) override;
};

#endif  // CHROME_BROWSER_LACROS_SCREEN_ORIENTATION_DELEGATE_LACROS_H_
