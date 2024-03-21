// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_UTIL_H_

#include "chromeos/crosapi/mojom/full_restore.mojom.h"

namespace sessions {
struct SessionWindow;
}

namespace full_restore {

// Converts a `sessions::SessionWindow` object to the one defined in
// full_restore.mojom.
crosapi::mojom::SessionWindowPtr ToSessionWindowPtr(
    sessions::SessionWindow* session_window);

}  // namespace full_restore

#endif  // CHROME_BROWSER_CHROMEOS_FULL_RESTORE_FULL_RESTORE_UTIL_H_
