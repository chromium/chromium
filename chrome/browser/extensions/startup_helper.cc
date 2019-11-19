// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/startup_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/initialize_extensions_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/extension.h"
#include "extensions/common/verifier_formats.h"

using content::BrowserThread;

namespace extensions {

namespace {

void PrintPackExtensionMessage(const std::string& message) {
  VLOG(1) << message;
}

}  // namespace

StartupHelper::StartupHelper() : pack_job_succeeded_(false) {
  EnsureExtensionsClientInitialized();
}

void StartupHelper::OnPackSuccess(
    const base::FilePath& crx_path,
    const base::FilePath& output_private_key_path) {
  pack_job_succeeded_ = true;
  PrintPackExtensionMessage(
      base::UTF16ToUTF8(
          PackExtensionJob::StandardSuccessMessage(crx_path,
                                                   output_private_key_path)));
}

void StartupHelper::OnPackFailure(const std::string& error_message,
                                  ExtensionCreator::ErrorType type) {
  PrintPackExtensionMessage(error_message);
}

bool StartupHelper::PackExtension(const base::CommandLine& cmd_line) {
  if (!cmd_line.HasSwitch(::switches::kPackExtension))
    return false;

  // Input Paths.
  base::FilePath src_dir =
      cmd_line.GetSwitchValuePath(::switches::kPackExtension);
  base::FilePath private_key_path;
  if (cmd_line.HasSwitch(::switches::kPackExtensionKey)) {
    private_key_path =
        cmd_line.GetSwitchValuePath(::switches::kPackExtensionKey);
  }

  // Launch a job to perform the packing on the blocking thread.  Ignore
  // warnings from the packing process. (e.g. Overwrite any existing crx file.)
  PackExtensionJob pack_job(this, src_dir, private_key_path,
                            ExtensionCreator::kOverwriteCRX);
  pack_job.set_synchronous();
  pack_job.Start();

  return pack_job_succeeded_;
}

namespace {

class ValidateCrxHelper : public SandboxedUnpackerClient {
 public:
  ValidateCrxHelper(const CRXFileInfo& file,
                    const base::FilePath& temp_dir,
                    base::OnceClosure quit_closure)
      : crx_file_(file),
        temp_dir_(temp_dir),
        quit_closure_(std::move(quit_closure)),
        success_(false) {}

  bool success() const { return success_; }
  const base::string16& error() const { return error_; }

  void Start() {
    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ValidateCrxHelper::StartOnBlockingThread, this));
  }

 protected:
  ~ValidateCrxHelper() override {}

  void OnUnpackSuccess(
      const base::FilePath& temp_dir,
      const base::FilePath& extension_root,
      std::unique_ptr<base::DictionaryValue> original_manifest,
      const Extension* extension,
      const SkBitmap& install_icon,
      const base::Optional<int>& dnr_ruleset_checksum) override {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
    success_ = true;
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&ValidateCrxHelper::FinishOnUIThread, this));
  }

  void OnUnpackFailure(const CrxInstallError& error) override {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
    success_ = false;
    error_ = error.message();
    base::PostTask(FROM_HERE, {BrowserThread::UI},
                   base::BindOnce(&ValidateCrxHelper::FinishOnUIThread, this));
  }

  void FinishOnUIThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::move(quit_closure_).Run();
  }

  void StartOnBlockingThread() {
    DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
    auto unpacker = base::MakeRefCounted<SandboxedUnpacker>(
        Manifest::INTERNAL, 0, /* no special creation flags */
        temp_dir_, GetExtensionFileTaskRunner().get(), this);
    unpacker->StartWithCrx(crx_file_);
  }

  // The file being validated.
  const CRXFileInfo& crx_file_;

  // The temporary directory where the sandboxed unpacker will do work.
  const base::FilePath& temp_dir_;

  // Closure called upon completion.
  base::OnceClosure quit_closure_;

  // Whether the unpacking was successful.
  bool success_;

  // If the unpacking wasn't successful, this contains an error message.
  base::string16 error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ValidateCrxHelper);
};

}  // namespace

bool StartupHelper::ValidateCrx(const base::CommandLine& cmd_line,
                                std::string* error) {
  CHECK(error);
  base::FilePath path = cmd_line.GetSwitchValuePath(::switches::kValidateCrx);
  if (path.empty()) {
    *error = base::StringPrintf("Empty path passed for %s",
                                ::switches::kValidateCrx);
    return false;
  }
  base::ScopedTempDir temp_dir;

  if (!temp_dir.CreateUniqueTempDir()) {
    *error = std::string("Failed to create temp dir");
    return false;
  }

  base::RunLoop run_loop;
  CRXFileInfo file(path, extensions::GetExternalVerifierFormat());
  auto helper = base::MakeRefCounted<ValidateCrxHelper>(
      file, temp_dir.GetPath(), run_loop.QuitClosure());
  helper->Start();
  run_loop.Run();

  bool success = helper->success();
  if (!success)
    *error = base::UTF16ToUTF8(helper->error());
  return success;
}

StartupHelper::~StartupHelper() {}

}  // namespace extensions
