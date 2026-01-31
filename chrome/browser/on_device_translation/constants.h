// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_CONSTANTS_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_CONSTANTS_H_

#include <cstddef>

namespace on_device_translation {

// The maximum number of pending tasks in the task queue in
// OnDeviceTranslationServiceController. When the number of pending tasks will
// exceed this limit, the request will fail.
extern const size_t kMaxPendingTaskCount;

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_CONSTANTS_H_
