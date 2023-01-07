// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SANDBOXED_UNPACKER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SANDBOXED_UNPACKER_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_status.h"
#include "chrome/browser/ash/system_extensions/system_extensions_status_or.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace ash {

class SystemExtensionsSandboxedUnpacker {
 public:
  SystemExtensionsSandboxedUnpacker();
  SystemExtensionsSandboxedUnpacker(const SystemExtensionsSandboxedUnpacker&) =
      delete;
  SystemExtensionsSandboxedUnpacker& operator=(
      const SystemExtensionsSandboxedUnpacker&) = delete;
  ~SystemExtensionsSandboxedUnpacker();

  using GetSystemExtensionFromCallback =
      base::OnceCallback<void(InstallStatusOrSystemExtension)>;

  // Attempts to create a SystemExtension object from the manifest in
  // `system_extension_dir`.
  void GetSystemExtensionFromDir(base::FilePath system_extension_dir,
                                 GetSystemExtensionFromCallback callback);

  // Attempts to create a SystemExtension object from a manifest string.
  void GetSystemExtensionFromString(
      base::StringPiece system_extension_manifest_string,
      GetSystemExtensionFromCallback callback);

  // Attempts to create a SystemExtension object from a parsed manifest
  // Value::Dict.
  InstallStatusOrSystemExtension GetSystemExtensionFromValue(
      const base::Value::Dict& parsed_manifest);

 private:
  // Helper class to run blocking IO operations on a separate thread.
  class IOHelper {
   public:
    ~IOHelper();

    SystemExtensionsStatusOr<SystemExtensionsInstallStatus, std::string>
    ReadManifestInDirectory(const base::FilePath& system_extension_dir);
  };

  void OnSystemExtensionManifestRead(
      GetSystemExtensionFromCallback callback,
      SystemExtensionsStatusOr<SystemExtensionsInstallStatus, std::string>
          result);

  void OnSystemExtensionManifestParsed(
      GetSystemExtensionFromCallback callback,
      data_decoder::DataDecoder::ValueOrError value_or_error);

  base::SequenceBound<IOHelper> io_helper_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_VISIBLE})};

  base::WeakPtrFactory<SystemExtensionsSandboxedUnpacker> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SANDBOXED_UNPACKER_H_
