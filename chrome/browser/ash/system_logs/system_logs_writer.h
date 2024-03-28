// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_SYSTEM_LOGS_WRITER_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_SYSTEM_LOGS_WRITER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

// Helper function for writing system logs used in Feedback reports. Currently
// used by chrome://net-internals#chromeos for manual uploading of system logs.

class Profile;

namespace ash {
namespace system_logs_writer {

// Writes system_logs.txt.zip to |dest_dir|, containing the contents from
// Feedback reports. If |scrub_data| is true then the logs are scrubbed of PII.
// Runs |callback| on completion with the complete file path on success, or
// nullopt on failure.
void WriteSystemLogs(
    Profile* profile,
    const base::FilePath& dest_dir,
    bool scrub_data,
    base::OnceCallback<void(std::optional<base::FilePath>)> callback);

}  // namespace system_logs_writer
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_SYSTEM_LOGS_WRITER_H_
