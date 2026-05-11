// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AT_MEMORY_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AT_MEMORY_ACCESSORY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/keyboard_accessory/android/accessory_controller.h"

namespace content {
class WebContents;
}

// Interface for the @memory action in the keyboard accessory.
class AtMemoryAccessoryController : public AccessoryController {
 public:
  AtMemoryAccessoryController() = default;
  AtMemoryAccessoryController(const AtMemoryAccessoryController&) = delete;
  AtMemoryAccessoryController& operator=(const AtMemoryAccessoryController&) =
      delete;
  ~AtMemoryAccessoryController() override = default;

  // Returns a reference to the unique AtMemoryAccessoryController associated
  // with `web_contents`. A new instance is created the first time this function
  // is called.
  static AtMemoryAccessoryController* GetOrCreate(
      content::WebContents* web_contents);

  virtual base::WeakPtr<AtMemoryAccessoryController> AsWeakPtr() = 0;
};

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_AT_MEMORY_ACCESSORY_CONTROLLER_H_
