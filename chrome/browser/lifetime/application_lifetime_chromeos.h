// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_CHROMEOS_H_
#define CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_CHROMEOS_H_

namespace chrome {

// Requests a relaunch from ChromeOS update engine.
// Only use this if there's an update pending.
void RelaunchForUpdate();

// True if there's a system update pending.
bool UpdatePending();

}  // namespace chrome

#endif  // CHROME_BROWSER_LIFETIME_APPLICATION_LIFETIME_CHROMEOS_H_
