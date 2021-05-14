// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_

#include "base/callback.h"
#include "storage/browser/file_system/async_file_util.h"

namespace chromeos {
namespace file_system_provider {

typedef base::OnceClosure AbortCallback;

}  // namespace file_system_provider
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace file_system_provider {
using ::chromeos::file_system_provider::AbortCallback;
}  // namespace file_system_provider
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_ABORT_CALLBACK_H_
