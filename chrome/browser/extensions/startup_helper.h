// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_STARTUP_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_STARTUP_HELPER_H_

#include <string>

#include "chrome/browser/extensions/pack_extension_job.h"

namespace base {
class CommandLine;
}

namespace extensions {

// Initialization helpers for various Extension startup actions.
class StartupHelper : public PackExtensionJob::Client {
 public:
  StartupHelper();

  StartupHelper(const StartupHelper&) = delete;
  StartupHelper& operator=(const StartupHelper&) = delete;

  ~StartupHelper() override;

  void OnPackSuccess(const base::FilePath& crx_path,
                     const base::FilePath& output_private_key_path) override;
  void OnPackFailure(const std::string& error_message,
                     ExtensionCreator::ErrorType type) override;

  // Handle --pack-extension flag from the |cmd_line| by packing the specified
  // extension. Returns false if the pack job failed.
  // If the return value is false, a description of the problem may be written
  // into |error|.
  bool PackExtension(const base::CommandLine& cmd_line, std::string* error);

  // Validates a crx at the path given by the --validate-extension flag - can
  // it be installed? Returns true if the crx is valid, or false otherwise.
  // If the return value is false, a description of the problem may be written
  // into |error|.
  bool ValidateCrx(const base::CommandLine& cmd_line, std::string* error);

 private:
  bool pack_job_succeeded_ = false;
  std::string error_message_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_STARTUP_HELPER_H_
