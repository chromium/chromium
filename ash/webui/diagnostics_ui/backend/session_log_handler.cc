// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/session_log_handler.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_client.h"
#include "ash/system/diagnostics/diagnostics_log_controller.h"
#include "ash/system/diagnostics/networking_log.h"
#include "ash/system/diagnostics/routine_log.h"
#include "ash/system/diagnostics/telemetry_log.h"
#include "ash/webui/diagnostics_ui/backend/session_log_async_helper.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace ash {
namespace diagnostics {
namespace {

const char kDefaultSessionLogFileName[] = "session_log.txt";

}  // namespace

SessionLogHandler::SessionLogHandler(
    const SelectFilePolicyCreator& select_file_policy_creator,
    ash::HoldingSpaceClient* holding_space_client,
    const base::FilePath& log_directory_path)
    : SessionLogHandler(select_file_policy_creator,
                        std::make_unique<TelemetryLog>(),
                        std::make_unique<RoutineLog>(log_directory_path),
                        std::make_unique<NetworkingLog>(log_directory_path),
                        holding_space_client) {}

SessionLogHandler::SessionLogHandler(
    const SelectFilePolicyCreator& select_file_policy_creator,
    std::unique_ptr<TelemetryLog> telemetry_log,
    std::unique_ptr<RoutineLog> routine_log,
    std::unique_ptr<NetworkingLog> networking_log,
    ash::HoldingSpaceClient* holding_space_client)
    : select_file_policy_creator_(select_file_policy_creator),
      telemetry_log_(std::move(telemetry_log)),
      routine_log_(std::move(routine_log)),
      networking_log_(std::move(networking_log)),
      holding_space_client_(holding_space_client),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      async_helper_(std::unique_ptr<ash::diagnostics::SessionLogAsyncHelper,
                                    base::OnTaskRunnerDeleter>(
          new SessionLogAsyncHelper(),
          base::OnTaskRunnerDeleter(task_runner_))) {
  DCHECK(holding_space_client_);
  weak_ptr_ = weak_factory_.GetWeakPtr();
  DETACH_FROM_SEQUENCE(session_log_handler_sequence_checker_);
}

SessionLogHandler::~SessionLogHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(session_log_handler_sequence_checker_);
  if (select_file_dialog_) {
    /* Lifecycle for SelectFileDialog is responsibility of calling code. */
    select_file_dialog_->ListenerDestroyed();
  }
}

void SessionLogHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::BindRepeating(&SessionLogHandler::HandleInitialize, weak_ptr_));
  web_ui()->RegisterMessageCallback(
      "saveSessionLog",
      base::BindRepeating(&SessionLogHandler::HandleSaveSessionLogRequest,
                          weak_ptr_));
}

void SessionLogHandler::FileSelected(const ui::SelectedFileInfo& file,
                                     int index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(session_log_handler_sequence_checker_);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &DiagnosticsLogController::GenerateSessionLogOnBlockingPool,
          // base::Unretained safe here because ~DiagnosticsLogController is
          // called during shutdown of ash::Shell and will out-live
          // SessionLogHandler.
          base::Unretained(DiagnosticsLogController::Get()), file.path()),
      base::BindOnce(&SessionLogHandler::OnSessionLogCreated, weak_ptr_,
                     file.path()));
  select_file_dialog_.reset();
}

void SessionLogHandler::FileSelectionCanceled() {
  RejectJavascriptCallback(save_session_log_callback_id_,
                           /*response=*/false);
  save_session_log_callback_id_ = "";
  select_file_dialog_.reset();
}

void SessionLogHandler::OnSessionLogCreated(const base::FilePath& file_path,
                                            bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(session_log_handler_sequence_checker_);
  if (success) {
    holding_space_client_->AddItemOfType(
        HoldingSpaceItem::Type::kDiagnosticsLog, file_path);
  }

  ResolveJavascriptCallback(save_session_log_callback_id_, success);
  save_session_log_callback_id_ = "";

  if (log_created_closure_)
    std::move(log_created_closure_).Run();
}

TelemetryLog* SessionLogHandler::GetTelemetryLog() const {
  return telemetry_log_.get();
}

RoutineLog* SessionLogHandler::GetRoutineLog() const {
  return routine_log_.get();
}

NetworkingLog* SessionLogHandler::GetNetworkingLog() const {
  return networking_log_.get();
}

void SessionLogHandler::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  task_runner_ = std::move(task_runner.get());
}

void SessionLogHandler::SetWebUIForTest(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

void SessionLogHandler::SetLogCreatedClosureForTest(base::OnceClosure closure) {
  log_created_closure_ = std::move(closure);
}

void SessionLogHandler::HandleSaveSessionLogRequest(
    const base::Value::List& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(session_log_handler_sequence_checker_);
  CHECK_EQ(1U, args.size());
  DCHECK(save_session_log_callback_id_.empty());
  CHECK(args[0].is_string());

  save_session_log_callback_id_ = args[0].GetString();

  content::WebContents* web_contents = web_ui()->GetWebContents();
  gfx::NativeWindow owning_window =
      web_contents ? web_contents->GetTopLevelNativeWindow()
                   : gfx::NativeWindow();

  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, select_file_policy_creator_.Run(web_contents));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      /*title=*/l10n_util::GetStringUTF16(IDS_DIAGNOSTICS_SELECT_DIALOG_TITLE),
      /*default_path=*/base::FilePath(kDefaultSessionLogFileName),
      /*file_types=*/nullptr,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(), owning_window);
}

void SessionLogHandler::HandleInitialize(const base::Value::List& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(session_log_handler_sequence_checker_);
  DCHECK(args.empty());
  AllowJavascript();
}

}  // namespace diagnostics
}  // namespace ash
