// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ACCELERATOR_UTILS_H_
#define CHROME_BROWSER_UI_ACCELERATOR_UTILS_H_

class Profile;

namespace ui {
class Accelerator;
}

namespace chrome {

// Returns true if the given |accelerator| is currently registered by
// Chrome.
bool IsChromeAccelerator(const ui::Accelerator& accelerator, Profile* profile);

ui::Accelerator GetPrimaryChromeAcceleratorForBookmarkTab();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ACCELERATOR_UTILS_H_
