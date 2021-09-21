// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ASYNC_LOG_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ASYNC_LOG_H_

#include "base/files/file_path.h"

namespace ash {
namespace diagnostics {

// `AsyncLog` is a simple test logger that appends lines/strings to the end of
// a file. The file is created on first write, and all IO operations occur
// on a `SequencedTaskRunner`.
//
// TODO(zentaro): Implement file operations.
class AsyncLog {
 public:
  explicit AsyncLog(const base::FilePath& file_path);
  AsyncLog(const AsyncLog&) = delete;
  AsyncLog& operator=(const AsyncLog&) = delete;
  ~AsyncLog();

 private:
  // Path of the log file.
  const base::FilePath file_path_;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ASYNC_LOG_H_
