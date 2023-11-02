// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_DELEGATE_H_

namespace extensions {

class ChromeAppIcon;

class ChromeAppIconDelegate {
 public:
  // Invoked when ChromeAppIcon is updated. |icon->image_skia()| contains
  // the update icon image with applied effects.
  virtual void OnIconUpdated(ChromeAppIcon* icon) = 0;

 protected:
  virtual ~ChromeAppIconDelegate() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_DELEGATE_H_
