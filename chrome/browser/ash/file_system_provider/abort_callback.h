// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_

#include "base/functional/callback.h"
#include "storage/browser/file_system/async_file_util.h"

namespace ash::file_system_provider {

typedef base::OnceClosure AbortCallback;

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_
