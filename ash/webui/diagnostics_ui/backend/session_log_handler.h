// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_

#include <memory>
#include <string>

#include "ash/webui/diagnostics_ui/backend/session_log_async_helper.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace content {
class WebContents;
}  // namespace content

namespace base {
class FilePath;
}  // namespace base

namespace ash {
class HoldingSpaceClient;
}  // namespace ash

namespace ash {
namespace diagnostics {

class TelemetryLog;
class RoutineLog;
class NetworkingLog;

class SessionLogHandler : public content::WebUIMessageHandler,
                          public ui::SelectFileDialog::Listener {
 public:
  using SelectFilePolicyCreator =
      base::RepeatingCallback<std::unique_ptr<ui::SelectFilePolicy>(
          content::WebContents*)>;
  SessionLogHandler(const SelectFilePolicyCreator& select_file_policy_creator,
                    ash::HoldingSpaceClient* holding_space_client,
                    const base::FilePath& log_directory_path);

  // Constructor for testing. Should not be called outside of tests.
  SessionLogHandler(const SelectFilePolicyCreator& select_file_policy_creator,
                    std::unique_ptr<TelemetryLog> telemetry_log,
                    std::unique_ptr<RoutineLog> routine_log,
                    std::unique_ptr<NetworkingLog> networking_log,
                    ash::HoldingSpaceClient* holding_space_client);

  ~SessionLogHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  void OnSessionLogCreated(const base::FilePath& path, bool success);

  SessionLogHandler(const SessionLogHandler&) = delete;
  SessionLogHandler& operator=(const SessionLogHandler&) = delete;

  TelemetryLog* GetTelemetryLog() const;
  RoutineLog* GetRoutineLog() const;
  NetworkingLog* GetNetworkingLog() const;

  // Sets the task runner to use for testing.
  void SetTaskRunnerForTesting(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  void SetWebUIForTest(content::WebUI* web_ui);
  void SetLogCreatedClosureForTest(base::OnceClosure closure);

 private:
  // Opens the select dialog.
  void HandleSaveSessionLogRequest(const base::Value::List& args);

  // Initializes Javascript.
  void HandleInitialize(const base::Value::List& args);

  SelectFilePolicyCreator select_file_policy_creator_;
  std::unique_ptr<TelemetryLog> telemetry_log_;
  std::unique_ptr<RoutineLog> routine_log_;
  std::unique_ptr<NetworkingLog> networking_log_;
  const raw_ptr<ash::HoldingSpaceClient> holding_space_client_;
  std::string save_session_log_callback_id_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  base::OnceClosure log_created_closure_;
  // Task runner for tasks posted by save session log handler. Used to ensure
  // posted tasks are handled while SessionLogHandler is in scope to stop
  // heap-use-after-free error.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<SessionLogAsyncHelper, base::OnTaskRunnerDeleter>
      async_helper_;
  SEQUENCE_CHECKER(session_log_handler_sequence_checker_);

  base::WeakPtr<SessionLogHandler> weak_ptr_;
  base::WeakPtrFactory<SessionLogHandler> weak_factory_{this};
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_SESSION_LOG_HANDLER_H_
