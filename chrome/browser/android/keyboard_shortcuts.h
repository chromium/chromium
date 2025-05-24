// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_KEYBOARD_SHORTCUTS_H_
#define CHROME_BROWSER_ANDROID_KEYBOARD_SHORTCUTS_H_

namespace ui {
class Accelerator;
}

namespace chrome::android {

// Returns whether the given accelerator is a chrome accelerator.
bool IsChromeAccelerator(const ui::Accelerator& accelerator);

}  // namespace chrome::android

#endif  // CHROME_BROWSER_ANDROID_KEYBOARD_SHORTCUTS_H_
