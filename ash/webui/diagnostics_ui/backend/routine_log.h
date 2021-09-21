// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ROUTINE_LOG_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ROUTINE_LOG_H_

#include <string>

#include "ash/webui/diagnostics_ui/backend/async_log.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"

namespace base {
class FilePath;
}

namespace ash {
namespace diagnostics {

// RoutineLog is used to record the status and outcome of Diagnostics Routines.
// Each time `LogRoutineStarted()` or `LogRoutineCompleted()` is called, a new
// line is appended to `routine_log_file_path`. The file is created before the
// first write if it does not exist.
class RoutineLog {
 public:
  explicit RoutineLog(const base::FilePath& routine_log_file_path);
  ~RoutineLog();

  RoutineLog(const RoutineLog&) = delete;
  RoutineLog& operator=(const RoutineLog&) = delete;

  // LogRoutine* functions schedule a task using sequence_task_runner_ to add an
  // entry in the routine log file. Tasks are run on blockable, sequenced task
  // runner to support I/O operations.
  void LogRoutineStarted(mojom::RoutineType type);
  void LogRoutineCompleted(mojom::RoutineType type,
                           mojom::StandardRoutineResult result);
  void LogRoutineCancelled();

  // Returns the current RoutineLog as a string.
  std::string GetContents() const;

 private:
  AsyncLog log_;
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_ROUTINE_LOG_H_
