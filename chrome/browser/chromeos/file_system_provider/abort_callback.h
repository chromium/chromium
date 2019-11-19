// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_

#include "base/callback.h"
#include "storage/browser/file_system/async_file_util.h"

namespace chromeos {
namespace file_system_provider {

typedef base::Closure AbortCallback;

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_
