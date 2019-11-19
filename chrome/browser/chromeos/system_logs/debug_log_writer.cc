// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/debug_log_writer.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/sequenced_task_runner.h"
#include "base/task/lazy_task_runner.h"
#include "base/task/post_task.h"
#include "chrome/common/logging_chrome.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

// Callback for returning status of executed external command.
typedef base::OnceCallback<void(bool succeeded)> CommandCompletionCallback;

const char kGzipCommand[] = "/bin/gzip";
const char kTarCommand[] = "/bin/tar";

base::LazySequencedTaskRunner g_sequenced_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::ThreadPool(),
                         base::MayBlock(),
                         base::TaskPriority::BEST_EFFORT,
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

// Called upon completion of |WriteDebugLogToFile|. Closes file
// descriptor, deletes log file in the case of failure and calls
// |callback|.
void WriteDebugLogToFileCompleted(const base::FilePath& file_path,
                                  DebugLogWriter::StoreLogsCallback callback,
                                  bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!succeeded) {
    bool posted = g_sequenced_task_runner.Get()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), file_path, false),
        base::BindOnce(std::move(callback), file_path, false));
    DCHECK(posted);
    return;
  }
  if (!callback.is_null())
    std::move(callback).Run(file_path, true);
}

// Stores into |file_path| debug logs in the .tgz format. Calls
// |callback| upon completion.
void WriteDebugLogToFile(std::unique_ptr<base::File> file,
                         const base::FilePath& file_path,
                         bool should_compress,
                         DebugLogWriter::StoreLogsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!file->IsValid()) {
    LOG(ERROR) << "Can't create debug log file: " << file_path.AsUTF8Unsafe()
               << ", "
               << "error: " << file->error_details();
    return;
  }
  chromeos::DBusThreadManager::Get()->GetDebugDaemonClient()->DumpDebugLogs(
      should_compress, file->GetPlatformFile(),
      base::BindOnce(&WriteDebugLogToFileCompleted, file_path,
                     std::move(callback)));

  // Close the file on an IO-allowed thread.
  g_sequenced_task_runner.Get()->DeleteSoon(FROM_HERE, file.release());
}

// Runs command with its parameters as defined in |argv|.
// Upon completion, it will report command run outcome via |callback| on the
// same thread from where it was initially called from.
void RunCommand(const std::vector<std::string>& argv,
                CommandCompletionCallback callback) {
  base::Process process = base::LaunchProcess(argv, base::LaunchOptions());
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to execute command " << argv[0];
    if (!callback.is_null())
      std::move(callback).Run(false);

    return;
  }

  int exit_code = 0;
  if (!process.WaitForExit(&exit_code)) {
    LOG(ERROR) << "Can't get exit code for pid " << process.Pid();
    if (!callback.is_null())
      std::move(callback).Run(false);

    return;
  }
  if (!callback.is_null())
    std::move(callback).Run(exit_code == 0);
}

// Callback for handling the outcome of CompressArchive(). It reports
// the final outcome of log retreival process at via |callback|.
void OnCompressArchiveCompleted(const base::FilePath& tar_file_path,
                                const base::FilePath& compressed_output_path,
                                DebugLogWriter::StoreLogsCallback callback,
                                bool compression_command_success) {
  if (!compression_command_success) {
    LOG(ERROR) << "Failed compressing " << compressed_output_path.value();
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(std::move(callback), base::FilePath(), false));
    base::DeleteFile(tar_file_path, false);
    base::DeleteFile(compressed_output_path, false);
    return;
  }

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(std::move(callback), compressed_output_path, true));
}

// Gzips |tar_file_path| and stores results in |compressed_output_path|.
void CompressArchive(const base::FilePath& tar_file_path,
                     const base::FilePath& compressed_output_path,
                     DebugLogWriter::StoreLogsCallback callback,
                     bool add_user_logs_command_success) {
  if (!add_user_logs_command_success) {
    LOG(ERROR) << "Failed adding user logs to " << tar_file_path.value();
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(std::move(callback), base::FilePath(), false));
    base::DeleteFile(tar_file_path, false);
    return;
  }

  std::vector<std::string> argv;
  argv.push_back(kGzipCommand);
  argv.push_back(tar_file_path.value());
  RunCommand(argv, base::BindOnce(&OnCompressArchiveCompleted, tar_file_path,
                                  compressed_output_path, std::move(callback)));
}

// Adds user sessions specific logs from |user_log_dir| into tar archive file
// at |tar_file_path|. Upon completion, it will call CompressArchive() to
// produce |compressed_output_path|.
void AddUserLogsToArchive(const base::FilePath& user_log_dir,
                          const base::FilePath& tar_file_path,
                          const base::FilePath& compressed_output_path,
                          DebugLogWriter::StoreLogsCallback callback) {
  std::vector<std::string> argv;
  argv.push_back(kTarCommand);
  argv.push_back("-rvf");
  argv.push_back(tar_file_path.value());
  argv.push_back(user_log_dir.value());
  RunCommand(argv, base::BindOnce(&CompressArchive, tar_file_path,
                                  compressed_output_path, std::move(callback)));
}

// Appends user logs after system logs are archived into |tar_file_path|.
void OnSystemLogsAdded(DebugLogWriter::StoreLogsCallback callback,
                       const base::FilePath& tar_file_path,
                       bool succeeded) {
  if (!succeeded) {
    if (!callback.is_null())
      std::move(callback).Run(base::FilePath(), false);

    return;
  }

  base::FilePath compressed_output_path =
      tar_file_path.AddExtension(FILE_PATH_LITERAL(".gz"));
  base::FilePath user_log_dir =
      logging::GetSessionLogDir(*base::CommandLine::ForCurrentProcess());

  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&AddUserLogsToArchive, user_log_dir, tar_file_path,
                     compressed_output_path, std::move(callback)));
}

void InitializeLogFile(base::File* file,
                       const base::FilePath& file_path,
                       uint32_t flags) {
  base::FilePath dir = file_path.DirName();
  if (!base::DirectoryExists(dir)) {
    if (!base::CreateDirectory(dir)) {
      LOG(ERROR) << "Can not create " << dir.value();
      return;
    }
  }

  file->Initialize(file_path, flags);
}

// Starts logs retrieval process. The output will be stored in file with name
// derived from |file_name_template|.
void StartLogRetrieval(const base::FilePath& file_name_template,
                       bool should_compress,
                       DebugLogWriter::StoreLogsCallback callback) {
  base::FilePath file_path =
      logging::GenerateTimestampedName(file_name_template, base::Time::Now());

  int flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
  std::unique_ptr<base::File> file(new base::File);
  base::File* file_ptr = file.get();
  g_sequenced_task_runner.Get()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&InitializeLogFile, base::Unretained(file_ptr), file_path,
                     flags),
      base::BindOnce(&WriteDebugLogToFile, base::Passed(&file), file_path,
                     should_compress, std::move(callback)));
}

}  // namespace

// static.
void DebugLogWriter::StoreLogs(const base::FilePath& fileshelf,
                               bool should_compress,
                               StoreLogsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::FilePath file_path =
      fileshelf.Append(should_compress ? FILE_PATH_LITERAL("debug-logs.tgz")
                                       : FILE_PATH_LITERAL("debug-logs.tar"));

  StartLogRetrieval(file_path, should_compress, std::move(callback));
}

// static.
void DebugLogWriter::StoreCombinedLogs(const base::FilePath& fileshelf,
                                       StoreLogsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  base::FilePath file_path =
      fileshelf.Append(FILE_PATH_LITERAL("combined-logs.tar"));

  // Get system logs from /var/log first, then add user-specific stuff.
  StartLogRetrieval(file_path, false,
                    base::BindOnce(&OnSystemLogsAdded, std::move(callback)));
}

}  // namespace chromeos
