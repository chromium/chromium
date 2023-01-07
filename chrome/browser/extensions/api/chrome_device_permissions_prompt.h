// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CHROME_DEVICE_PERMISSIONS_PROMPT_H_
#define CHROME_BROWSER_EXTENSIONS_API_CHROME_DEVICE_PERMISSIONS_PROMPT_H_

#include "extensions/browser/api/device_permissions_prompt.h"

class ChromeDevicePermissionsPrompt
    : public extensions::DevicePermissionsPrompt {
 public:
  explicit ChromeDevicePermissionsPrompt(content::WebContents* web_contents)
      : extensions::DevicePermissionsPrompt(web_contents) {}

  ~ChromeDevicePermissionsPrompt() override {}

 private:
  void ShowDialog() override;

  // Shows the Views permissions prompt specifically. This is a temporary hack
  // for MacViews; do not use this.
  void ShowDialogViews();
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_CHROME_DEVICE_PERMISSIONS_PROMPT_H_
