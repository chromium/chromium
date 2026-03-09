// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <unistd.h>

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "chrome/browser/ash/policy/reporting/single_arc_app_install_event_log.h"
#include "chrome/browser/ash/policy/reporting/single_extension_install_event_log.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace policy {

namespace {

base::File CreatePipeFileWithContents(base::span<const uint8_t> data) {
  int pipefd[2];
  CHECK_EQ(HANDLE_EINTR(pipe(pipefd)), 0);
  base::File pipe_read_end = base::File(base::ScopedFD(pipefd[0]));
  base::File pipe_write_end = base::File(base::ScopedFD(pipefd[1]));
  CHECK(pipe_write_end.WriteAtCurrentPosAndCheck(data));
  return pipe_read_end;
}

}  // namespace

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  {
    base::File file = CreatePipeFileWithContents(data);
    std::unique_ptr<SingleArcAppInstallEventLog> log;
    SingleArcAppInstallEventLog::Load(&file, &log);
  }

  {
    base::File file = CreatePipeFileWithContents(data);
    std::unique_ptr<SingleExtensionInstallEventLog> log;
    SingleExtensionInstallEventLog::Load(&file, &log);
  }

  return 0;
}

}  // namespace policy
