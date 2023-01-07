// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "chrome/browser/ash/policy/reporting/single_arc_app_install_event_log.h"
#include "chrome/browser/ash/policy/reporting/single_extension_install_event_log.h"

namespace policy {

namespace {

base::File CreatePipeFileWithContents(const uint8_t* data, size_t size) {
  int pipefd[2];
  CHECK_EQ(HANDLE_EINTR(pipe(pipefd)), 0);
  base::File pipe_read_end = base::File(base::ScopedFD(pipefd[0]));
  base::File pipe_write_end = base::File(base::ScopedFD(pipefd[1]));
  if (size) {
    CHECK(pipe_write_end.WriteAtCurrentPos(reinterpret_cast<const char*>(data),
                                           size));
  }
  return pipe_read_end;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  {
    base::File file = CreatePipeFileWithContents(data, size);
    std::unique_ptr<SingleArcAppInstallEventLog> log;
    SingleArcAppInstallEventLog::Load(&file, &log);
  }

  {
    base::File file = CreatePipeFileWithContents(data, size);
    std::unique_ptr<SingleExtensionInstallEventLog> log;
    SingleExtensionInstallEventLog::Load(&file, &log);
  }

  return 0;
}

}  // namespace policy
