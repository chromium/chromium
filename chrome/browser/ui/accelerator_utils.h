// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACCELERATOR_UTILS_H_
#define CHROME_BROWSER_UI_ACCELERATOR_UTILS_H_

class Browser;

namespace ui {
class Accelerator;
class AcceleratorProvider;
}

namespace chrome {

// Returns true if the given |accelerator| is currently registered by
// Chrome.
bool IsChromeAccelerator(const ui::Accelerator& accelerator);
// Returns the AcceleratorProvider associated with |browser|, or nullptr
// if one is not available.
ui::AcceleratorProvider* AcceleratorProviderForBrowser(Browser* browser);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ACCELERATOR_UTILS_H_
