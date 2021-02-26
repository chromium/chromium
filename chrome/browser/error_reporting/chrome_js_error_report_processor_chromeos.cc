// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

#include <errno.h>

#include <algorithm>

#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/error_reporting/constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// Per the memfd_create man page, we need _GNU_SOURCE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/mman.h>

namespace {

// The format used to communicate with crash_reporter keys treats ':' as special
// (introducing the length of the value). Remove any :'s from the key names in
// place.
void ReplaceColonsWithUnderscores(std::string& key) {
  std::replace(key.begin(), key.end(), ':', '_');
}

// Gets a File pointing to some temporary location. In some cases, we have to
// do extra cleanup; pass an *unbound* ScopedClosureRunner in |cleanup| so that
// this function can add the necessary cleanup function.
// If |force_non_memfd_for_test| is true, we act as if the memfd call failed and
// go to the temp file case. Since most machines have memfd_create implemented,
// this is the only way to get some unit-test coverage on the non-memfd_create
// path.
base::File GetMemfdOrTempFile(base::ScopedClosureRunner& cleanup,
                              bool force_non_memfd_for_test) {
  DCHECK(!cleanup) << "cleanup must be unbound";
  if (!force_non_memfd_for_test) {
    int memfd = HANDLE_EINTR(memfd_create("javascript_error", 0));
    if (memfd != -1) {
      return base::File(memfd);
    }

    if (errno != ENOSYS) {
      PLOG(ERROR)
          << "Could not create memfd file for JavaScript error reporting";
      return base::File(base::File::FILE_ERROR_FAILED);
    }
  }

  // Note that some VMs and boards with old kernels don't have memfd_create
  // implemented yet. Work around by creating a temp file.
  base::FilePath output_path;
  base::ScopedFILE output_file = CreateAndOpenTemporaryStream(&output_path);
  if (!output_file) {
    PLOG(ERROR)
        << "memfd_create not implemented and cannot create temporary stream";
    return base::File(base::File::FILE_ERROR_FAILED);
  }

  DLOG(WARNING) << "JavaScript error reporting: Falling back to temp file "
                << output_path.value();

  // Need to actually delete the temp file once we're done.
  cleanup.ReplaceClosure(
      base::BindOnce(base::GetDeleteFileCallback(), std::move(output_path)));
  return base::FILEToFile(output_file.release());
}

}  // namespace

std::vector<std::string>
ChromeJsErrorReportProcessor::GetCrashReporterArgvStart() {
  return {"/sbin/crash_reporter"};
}

std::string ChromeJsErrorReportProcessor::ParamsToCrashReporterString(
    const ParameterMap& params,
    const base::Optional<std::string>& stack_trace) {
  std::string result;
  for (const auto& param : params) {
    std::string key = param.first;
    const std::string& value = param.second;
    ReplaceColonsWithUnderscores(key);
    std::string value_length_string = base::NumberToString(value.length());
    base::StrAppend(&result, {key, ":", value_length_string, ":", value});
  }
  if (stack_trace) {
    const std::string& payload = stack_trace.value();

    std::string value_length_string = base::NumberToString(payload.length());
    base::StrAppend(
        &result, {kJavaScriptStackKey, ":", value_length_string, ":", payload});
  }

  return result;
}

void ChromeJsErrorReportProcessor::SendReportViaCrashReporter(
    ParameterMap params,
    base::Optional<std::string> stack_trace) {
  base::ScopedClosureRunner cleanup;
  base::File output(GetMemfdOrTempFile(cleanup, force_non_memfd_for_test_));
  if (!output.IsValid()) {
    return;  // Already logged error message in GetMemfdOrTempFile.
  }

  std::string string_to_write =
      ParamsToCrashReporterString(params, stack_trace);
  if (output.WriteAtCurrentPos(string_to_write.data(),
                               string_to_write.length()) !=
      static_cast<int>(string_to_write.length())) {
    PLOG(ERROR) << "Failed to write to crash_reporter pipe";
    return;
  }

  base::LaunchOptions crash_reporter_options;
  crash_reporter_options.fds_to_remap.emplace_back(output.GetPlatformFile(),
                                                   output.GetPlatformFile());

  std::vector<std::string> argv(GetCrashReporterArgvStart());
  argv.insert(argv.end(),
              {base::StrCat({"--chrome_memfd=",
                             base::NumberToString(output.GetPlatformFile())}),
               base::StrCat({"--pid=", base::NumberToString(getpid())}),
               base::StrCat({"--uid=", base::NumberToString(geteuid())}),
               "--error_key=jserror"});

  base::Process process = base::LaunchProcess(argv, crash_reporter_options);
  if (!process.IsValid()) {
    PLOG(ERROR) << "Failed to launch " << base::JoinString(argv, " ");
    return;
  }

  {
    base::ScopedAllowBaseSyncPrimitives allow_wait_for_exit;
    // Wait for crash_reporter to finish. We need to wait for it to finish
    // before we delete the temporary files that may have been created in
    // GetMemfdOrTempFile().
    int return_code = 0;
    constexpr base::TimeDelta kMaximumWait = base::TimeDelta::FromMinutes(1);
    if (process.WaitForExitWithTimeout(kMaximumWait, &return_code)) {
      if (return_code != 0) {
        LOG(WARNING) << "crash_reporter subprocess failed with return value "
                     << return_code
                     << (return_code == -1 ? " or maybe crashed" : "");
      }
    } else {
      // Kill the stuck process to avoid zombies.
      LOG(WARNING) << "crash_reporter failed to complete within "
                   << kMaximumWait;
      process.Terminate(0, false /*wait*/);
    }
  }
}

void ChromeJsErrorReportProcessor::SendReport(
    ParameterMap params,
    base::Optional<std::string> stack_trace,
    bool send_to_production_servers,
    base::ScopedClosureRunner callback_runner,
    base::Time report_time,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  // On Chrome OS, send the report through the OS crash reporting system to
  // get more metadata and to keep all the consent logic in one place. We need
  // to do file I/O, so over to a blockable thread for the send and then back to
  // the UI thread for the finished callback.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ChromeJsErrorReportProcessor::SendReportViaCrashReporter,
                     this, std::move(params), std::move(stack_trace)),
      callback_runner.Release());
}
