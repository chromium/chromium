// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/shared_command_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/chrome_management_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/metrics_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  auto* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line ||
      !command_line->HasSwitch(enterprise_connectors::switches::kPipeName)) {
    enterprise_connectors::RecordError(
        enterprise_connectors::ManagementServiceError::kCommandMissingPipeName);
    SYSLOG(ERROR) << "The chrome-management-service failed. Invalid command, "
                  << "missing details to connect to the browser process.";
    return enterprise_connectors::kFailure;
  }

  uint64_t pipe_name;
  if (!base::StringToUint64(command_line->GetSwitchValueNative(
                                enterprise_connectors::switches::kPipeName),
                            &pipe_name)) {
    enterprise_connectors::RecordError(
        enterprise_connectors::ManagementServiceError::
            kPipeNameRetrievalFailure);
    SYSLOG(ERROR) << "The chrome-management-service failed. Could not "
                  << "correctly retrieve the details to connect to the browser "
                  << "process.";
    return enterprise_connectors::kFailure;
  }

  // Initializes the Mojo scoped IPC thread for the current process inorder
  // to use Mojo IPC and connect to the browser process.
  mojo::core::Init();
  base::Thread ipc_thread("Mojo IPC");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);

  enterprise_connectors::ChromeManagementService chrome_management_service;
  return chrome_management_service.Run(command_line, pipe_name);
}
