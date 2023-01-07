// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_zipfile_installer.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {

ZipFileInstaller::DoneCallback MakeRegisterInExtensionServiceCallback(
    ExtensionService* service) {
  return base::BindOnce(
      [](base::WeakPtr<ExtensionService> extension_service_weak,
         const base::FilePath& zip_file, const base::FilePath& unzip_dir,
         const std::string& error) {
        if (!extension_service_weak)
          return;

        if (!unzip_dir.empty()) {
          DCHECK(error.empty());
          UnpackedInstaller::Create(extension_service_weak.get())
              ->Load(unzip_dir);
          return;
        }
        DCHECK(!error.empty());
        LoadErrorReporter::GetInstance()->ReportLoadError(
            zip_file, error, extension_service_weak->profile(),
            /*noisy_on_failure=*/true);
      },
      service->AsExtensionServiceWeakPtr());
}

}  // namespace extensions
