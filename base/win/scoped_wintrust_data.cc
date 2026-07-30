// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_wintrust_data.h"

#include <windows.h>

#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"

namespace base::win {

struct ScopedWintrustData::Impl {
  std::wstring file_path;
  WINTRUST_FILE_INFO file_info{};
  WINTRUST_DATA wintrust_data{};
  LONG status = ERROR_GEN_FAILURE;
};

ScopedWintrustData::ScopedWintrustData(const FilePath& binary_path)
    : file_(binary_path,
            base::File::FLAG_OPEN | base::File::FLAG_READ |
                base::File::FLAG_WIN_SHARE_DELETE) {
  Initialize(binary_path, file_.IsValid() ? file_.GetPlatformFile() : nullptr);
}

ScopedWintrustData::ScopedWintrustData(const FilePath& binary_path,
                                       HANDLE file_handle) {
  Initialize(binary_path, file_handle);
}

void ScopedWintrustData::Initialize(const FilePath& binary_path,
                                    HANDLE file_handle) {
  // This function uses the WinVerifyTrust function to validate the signature
  // for the provided |binary_path|. More information on the structures and
  // function used here can be found at:
  // https://docs.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust
  DCHECK(!impl_);
  DCHECK(!binary_path.empty());
  DCHECK(file_handle != INVALID_HANDLE_VALUE);

  impl_ = std::make_unique<Impl>();
  impl_->file_path = binary_path.value();

  impl_->file_info.cbStruct = sizeof(WINTRUST_FILE_INFO);
  impl_->file_info.pcwszFilePath = impl_->file_path.c_str();
  impl_->file_info.hFile = file_handle;

  impl_->wintrust_data.cbStruct = sizeof(WINTRUST_DATA);
  impl_->wintrust_data.dwUIChoice = WTD_UI_NONE;
  impl_->wintrust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  impl_->wintrust_data.dwUnionChoice = WTD_CHOICE_FILE;
  impl_->wintrust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  impl_->wintrust_data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
  impl_->wintrust_data.dwUIContext = WTD_UICONTEXT_EXECUTE;
  impl_->wintrust_data.pFile = &impl_->file_info;

  GUID policy_guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  impl_->status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                                 &policy_guid, &impl_->wintrust_data);
}

ScopedWintrustData::~ScopedWintrustData() {
  // Free the provider data if verify action was performed.
  // We check hWVTStateData as it's allocated by the trust provider.
  if (impl_ && impl_->wintrust_data.hWVTStateData != nullptr) {
    GUID policy_guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    impl_->wintrust_data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &policy_guid,
                   &impl_->wintrust_data);
  }
}

LONG ScopedWintrustData::status() const {
  return impl_ ? impl_->status : ERROR_GEN_FAILURE;
}

bool ScopedWintrustData::is_valid() const {
  return status() == ERROR_SUCCESS;
}

HANDLE ScopedWintrustData::hWVTStateData() const {
  return impl_ ? impl_->wintrust_data.hWVTStateData : nullptr;
}

}  // namespace base::win
