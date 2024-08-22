// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_SELECTOR_STORAGE_SELECTOR_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_SELECTOR_STORAGE_SELECTOR_H_

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/util/statusor.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/policy/messaging_layer/util/upload_declarations.h"
#include "components/reporting/storage/storage_uploader_interface.h"  // nogncheck
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace base {
class FilePath;
}

namespace reporting {

// This static class facilitates `ReportingClient` ability to select underlying
// storage for encrypted reporting pipeline report client.  It is built into
// Chrome and configured differently depending on whether Chrome is intended for
// ChromeOS/LaCros or not and whether it is Ash Chrome: it can store event
// locally or in Missive Daemon. It can also be built into other daemons; in
// that case it always connects to Missive Daemon.
// This class is never instantiated; it serves as a front for the client
// configuration settings according to the build.
class StorageSelector {
 public:
  static bool is_use_missive();
  static bool is_uploader_required();

#if BUILDFLAG(IS_CHROMEOS)
  static void CreateMissiveStorageModule(
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
          cb);
#else  // !BUILDFLAG(IS_CHROMEOS)
  static void CreateLocalStorageModule(
      const base::FilePath& local_reporting_path,
      std::string_view verification_key,
      CompressionInformation::CompressionAlgorithm compression_algorithm,
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModuleInterface>>)>
          cb);

  static ReportSuccessfulUploadCallback GetLocalReportSuccessfulUploadCb(
      scoped_refptr<StorageModuleInterface> storage_module);

  static EncryptionKeyAttachedCallback GetLocalEncryptionKeyAttachedCb(
      scoped_refptr<StorageModuleInterface> storage_module);

#endif  // !BUILDFLAG(IS_CHROMEOS)
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_STORAGE_SELECTOR_STORAGE_SELECTOR_H_
