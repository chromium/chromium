// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SANDBOXED_UNPACKER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SANDBOXED_UNPACKER_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class SystemExtensionsSandboxedUnpacker {
 public:
  SystemExtensionsSandboxedUnpacker();
  SystemExtensionsSandboxedUnpacker(const SystemExtensionsSandboxedUnpacker&) =
      delete;
  SystemExtensionsSandboxedUnpacker& operator=(
      const SystemExtensionsSandboxedUnpacker&) = delete;
  ~SystemExtensionsSandboxedUnpacker();

  enum class Status {
    kOk,
    kFailedJsonErrorParsingManifest,
    kFailedIdMissing,
    kFailedIdInvalid,
    kFailedTypeMissing,
    kFailedTypeInvalid,
    kFailedServiceWorkerUrlMissing,
    kFailedServiceWorkerUrlInvalid,
    kFailedServiceWorkerUrlDifferentOrigin,
    kFailedNameMissing,
    kFailedNameEmpty,
  };

  // Attempts to create a SystemExtension object from a manifest string.
  using GetSystemExtensionFromStringCallback =
      base::OnceCallback<void(Status, std::unique_ptr<SystemExtension>)>;
  void GetSystemExtensionFromString(
      base::StringPiece system_extension_manifest_string,
      GetSystemExtensionFromStringCallback callback);

 private:
  void OnSystemExtensionManifestParsed(
      GetSystemExtensionFromStringCallback callback,
      data_decoder::DataDecoder::ValueOrError value_or_error);

  base::WeakPtrFactory<SystemExtensionsSandboxedUnpacker> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_SANDBOXED_UNPACKER_H_
