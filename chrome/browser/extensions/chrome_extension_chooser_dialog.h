// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_CHOOSER_DIALOG_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_CHOOSER_DIALOG_H_

#include <memory>

namespace content {
class WebContents;
}

namespace permissions {
class ChooserController;
}

void ShowConstrainedDeviceChooserDialog(
    content::WebContents* web_contents,
    std::unique_ptr<permissions::ChooserController> controller);

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_CHOOSER_DIALOG_H_
