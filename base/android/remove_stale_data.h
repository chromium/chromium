// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_REMOVE_STALE_DATA_H_
#define BASE_ANDROID_REMOVE_STALE_DATA_H_

#include "base/base_export.h"

namespace base {

class FilePath;

namespace android {

// Removes the `data_directory` with all its contents and records a histogram
// allowing to estimate the rate of removals.
// TODO(crbug.com/1331809): Remove this code after the data from the field shows
// no removal is happening in practice, plus a few milestones.
void BASE_EXPORT RemoveStaleDataDirectory(const base::FilePath& data_directory);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_REMOVE_STALE_DATA_H_
