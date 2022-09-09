// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_TEST_ICON_IMAGE_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_TEST_ICON_IMAGE_OBSERVER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "extensions/browser/extension_icon_image.h"

namespace extensions {

class Extension;

// This class helps to observe action icons. As default action icons load
// asynchronously we need to wait for it to finish before using them.
class TestIconImageObserver : public IconImage::Observer {
 public:
  TestIconImageObserver();

  TestIconImageObserver(const TestIconImageObserver&) = delete;
  TestIconImageObserver& operator=(const TestIconImageObserver&) = delete;

  ~TestIconImageObserver() override;

  void Wait(IconImage* icon);

  static void WaitForIcon(IconImage* icon);
  static void WaitForExtensionActionIcon(const Extension* extension,
                                         content::BrowserContext* context);

 private:
  // IconImage::Observer:
  void OnExtensionIconImageChanged(IconImage* icon) override;

  base::RunLoop run_loop_;
  base::ScopedObservation<IconImage, IconImage::Observer> observation_{this};
};
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_TEST_ICON_IMAGE_OBSERVER_H_
