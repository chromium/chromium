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
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/extensions/extension_util_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace extensions {

namespace {
struct CrxAndKeyFiles {
  base::FilePath crx;
  base::FilePath key;
};

std::optional<CrxAndKeyFiles> GetCrxAndKeyFilePaths(
    const base::FilePath& root_directory,
    const base::FilePath& key_file) {
  bool create_key_file = key_file.empty();

#if BUILDFLAG(IS_ANDROID)
  if (root_directory.IsVirtualDocumentPath()) {
    std::vector<std::string> file_extensions{kExtensionFileExtension};
    if (create_key_file) {
      file_extensions.push_back(kExtensionKeyFileExtension);
    }
    std::optional<std::vector<base::FilePath>> crx_key_files =
        GetOrCreateEmptyFilesUnderDownloads(root_directory, file_extensions);
    if (!crx_key_files) {
      return std::nullopt;
    }
    return CrxAndKeyFiles{(*crx_key_files)[0], create_key_file
                                                   ? (*crx_key_files)[1]
                                                   : base::FilePath()};
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return CrxAndKeyFiles{
      root_directory.AddExtension(kExtensionFileExtension),
      create_key_file ? root_directory.AddExtension(kExtensionKeyFileExtension)
                      : base::FilePath(),
  };
}
}  // namespace

PackExtensionJob::PackExtensionJob(Client* client,
                                   const base::FilePath& root_directory,
                                   const base::FilePath& key_file,
                                   int run_flags)
    : client_(client),
      key_file_(key_file),
      run_flags_(run_flags | ExtensionCreator::kRequireModernManifestVersion) {
  root_directory_ = root_directory.StripTrailingSeparators();
}

PackExtensionJob::~PackExtensionJob() = default;

void PackExtensionJob::Start() {
  if (run_mode_ == RunMode::kAsynchronous) {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::SequencedTaskRunner::GetCurrentDefault();
    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PackExtensionJob::Run,
                       // See class level comments for why Unretained is safe.
                       base::Unretained(this), std::move(task_runner)));
  } else {
    DCHECK_EQ(RunMode::kSynchronous, run_mode_);
    Run(nullptr);
  }
}

void PackExtensionJob::Run(
    scoped_refptr<base::SequencedTaskRunner> async_reply_task_runner) {
  DCHECK_EQ(!!async_reply_task_runner, run_mode_ == RunMode::kAsynchronous)
      << "Provide task runner iff we are running in asynchronous mode.";

  std::optional<CrxAndKeyFiles> files =
      GetCrxAndKeyFilePaths(root_directory_, key_file_);
  if (!files) {
    ReportFailureOnClientSequence(u"Failed to create files under Downloads",
                                  ExtensionCreator::ErrorType::kOtherError);
  }
  auto crx_file_out = std::make_unique<base::FilePath>(std::move(files->crx));
  auto key_file_out = std::make_unique<base::FilePath>(std::move(files->key));

  ExtensionCreator creator;
  if (creator.Run(root_directory_, *crx_file_out, key_file_, *key_file_out,
                  run_flags_)) {
    if (run_mode_ == RunMode::kAsynchronous) {
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
    if (run_mode_ == RunMode::kAsynchronous) {
      async_reply_task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(&PackExtensionJob::ReportFailureOnClientSequence,
                         // See class level comments for why Unretained is safe.
                         base::Unretained(this), creator.error_message(),
                         creator.error_type()));
    } else {
      DCHECK_EQ(RunMode::kSynchronous, run_mode_);
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
    const std::u16string& error,
    ExtensionCreator::ErrorType error_type) {
  DCHECK(client_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(crbug.com/41317803): Continue removing std::string errors and
  // replacing with std::u16string.
  client_->OnPackFailure(base::UTF16ToUTF8(error), error_type);
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
