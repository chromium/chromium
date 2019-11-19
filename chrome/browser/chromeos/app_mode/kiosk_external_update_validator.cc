// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_external_update_validator.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace chromeos {

KioskExternalUpdateValidator::KioskExternalUpdateValidator(
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    const extensions::CRXFileInfo& file,
    const base::FilePath& crx_unpack_dir,
    const base::WeakPtr<KioskExternalUpdateValidatorDelegate>& delegate)
    : backend_task_runner_(backend_task_runner),
      crx_file_(file),
      crx_unpack_dir_(crx_unpack_dir),
      delegate_(delegate) {
}

KioskExternalUpdateValidator::~KioskExternalUpdateValidator() {
}

void KioskExternalUpdateValidator::Start() {
  auto unpacker = base::MakeRefCounted<extensions::SandboxedUnpacker>(
      extensions::Manifest::EXTERNAL_PREF, extensions::Extension::NO_FLAGS,
      crx_unpack_dir_, backend_task_runner_.get(), this);
  if (!backend_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&extensions::SandboxedUnpacker::StartWithCrx,
                         unpacker.get(), crx_file_))) {
    NOTREACHED();
  }
}

void KioskExternalUpdateValidator::OnUnpackFailure(
    const extensions::CrxInstallError& error) {
  LOG(ERROR) << "Failed to unpack external kiosk crx file: "
             << crx_file_.extension_id << " " << error.message();
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &KioskExternalUpdateValidatorDelegate::OnExternalUpdateUnpackFailure,
          delegate_, crx_file_.extension_id));
}

void KioskExternalUpdateValidator::OnUnpackSuccess(
    const base::FilePath& temp_dir,
    const base::FilePath& extension_dir,
    std::unique_ptr<base::DictionaryValue> original_manifest,
    const extensions::Extension* extension,
    const SkBitmap& install_icon,
    const base::Optional<int>& dnr_ruleset_checksum) {
  DCHECK(crx_file_.extension_id == extension->id());

  std::string minimum_browser_version;
  if (!extension->manifest()->GetString(
          extensions::manifest_keys::kMinimumChromeVersion,
          &minimum_browser_version)) {
    LOG(ERROR) << "Can't find minimum browser version for app "
               << crx_file_.extension_id;
    minimum_browser_version.clear();
  }

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &KioskExternalUpdateValidatorDelegate::OnExternalUpdateUnpackSuccess,
          delegate_, crx_file_.extension_id, extension->VersionString(),
          minimum_browser_version, temp_dir));
}

}  // namespace chromeos
