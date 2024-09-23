// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pack_extension_job.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace extensions {

PackExtensionJob::PackExtensionJob(Client* client,
                                   const base::FilePath& root_directory,
                                   const base::FilePath& key_file,
                                   int run_flags)
    : client_(client),
      key_file_(key_file),
      run_flags_(run_flags | ExtensionCreator::kRequireModernManifestVersion) {
  root_directory_ = root_directory.StripTrailingSeparators();
}

PackExtensionJob::~PackExtensionJob() {}

void PackExtensionJob::Start() {
  if (run_mode_ == RunMode::ASYNCHRONOUS) {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PackExtensionJob::Run,
                       // See class level comments for why Unretained is safe.
                       base::Unretained(this), std::move(task_runner)));
  } else {
    DCHECK_EQ(RunMode::SYNCHRONOUS, run_mode_);
    Run(nullptr);
  }
}

void PackExtensionJob::Run(
    scoped_refptr<base::SequencedTaskRunner> async_reply_task_runner) {
  DCHECK_EQ(!!async_reply_task_runner, run_mode_ == RunMode::ASYNCHRONOUS)
      << "Provide task runner iff we are running in asynchronous mode.";
  auto crx_file_out = std::make_unique<base::FilePath>(
      root_directory_.AddExtension(kExtensionFileExtension));

  auto key_file_out = std::make_unique<base::FilePath>();
  if (key_file_.empty()) {
    *key_file_out = root_directory_.AddExtension(kExtensionKeyFileExtension);
  }

  ExtensionCreator creator;
  if (creator.Run(root_directory_, *crx_file_out, key_file_, *key_file_out,
                  run_flags_)) {
    if (run_mode_ == RunMode::ASYNCHRONOUS) {
      async_reply_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&PackExtensionJob::ReportSuccessOnClientSequence,
                         // See class level comments for why Unretained is safe.
                         base::Unretained(this), std::move(crx_file_out),
                         std::move(key_file_out)));

    } else {
      ReportSuccessOnClientSequence(std::move(crx_file_out),
                                    std::move(key_file_out));
    }
  } else {
    if (run_mode_ == RunMode::ASYNCHRONOUS) {
      async_reply_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&PackExtensionJob::ReportFailureOnClientSequence,
                         // See class level comments for why Unretained is safe.
                         base::Unretained(this), creator.error_message(),
                         creator.error_type()));
    } else {
      DCHECK_EQ(RunMode::SYNCHRONOUS, run_mode_);
      ReportFailureOnClientSequence(creator.error_message(),
                                    creator.error_type());
    }
  }
}

void PackExtensionJob::ReportSuccessOnClientSequence(
    std::unique_ptr<base::FilePath> crx_file_out,
    std::unique_ptr<base::FilePath> key_file_out) {
  DCHECK(client_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnPackSuccess(*crx_file_out, *key_file_out);
}

void PackExtensionJob::ReportFailureOnClientSequence(
    const std::string& error,
    ExtensionCreator::ErrorType error_type) {
  DCHECK(client_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnPackFailure(error, error_type);
}

// static
std::u16string PackExtensionJob::StandardSuccessMessage(
    const base::FilePath& crx_file,
    const base::FilePath& key_file) {
  std::u16string crx_file_string = crx_file.LossyDisplayName();
  std::u16string key_file_string = key_file.LossyDisplayName();
  if (key_file_string.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PACK_DIALOG_SUCCESS_BODY_UPDATE,
        crx_file_string);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_PACK_DIALOG_SUCCESS_BODY_NEW,
        crx_file_string,
        key_file_string);
  }
}

}  // namespace extensions
