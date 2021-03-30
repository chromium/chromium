// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_DEBUG_LOG_WRITER_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_DEBUG_LOG_WRITER_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"

namespace ash {
namespace debug_log_writer {

// Stores debug logs collected from debugd as a .tgz archive to |out_dir|.
// If |include_chrome_logs| is true, the Chrome user logs are included.
// |callback| is invoked on success with the full file path of the archive,
// or with nullopt on failure.
void StoreLogs(
    const base::FilePath& out_dir,
    bool include_chrome_logs,
    base::OnceCallback<void(base::Optional<base::FilePath> logs_path)>
        callback);

}  // namespace debug_log_writer
}  // namespace ash

// TODO(https://crbug.com/1164001): remove when Chrome OS code migration is
// done.
namespace chromeos {
namespace debug_log_writer {
using ::ash::debug_log_writer::StoreLogs;
}  // namespace debug_log_writer
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_DEBUG_LOG_WRITER_H_
