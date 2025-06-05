// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage_win.h"

#include <objbase.h>

#include <unknwn.h>
#include <windows.h>

#include <comutil.h>
#include <oleauto.h>
#include <winerror.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/google/google_update_app_command.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace policy {
namespace {

bool StoreDMTokenInRegistry(const std::string& token) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (token.empty())
    return false;

  auto app_command = GetUpdaterAppCommand(installer::kCmdStoreDMToken);
  if (!app_command.has_value()) {
    return false;
  }

  std::string token_base64 = base::Base64Encode(token);
  VARIANT var;
  VariantInit(&var);
  _variant_t token_var = token_base64.c_str();
  if (FAILED(app_command.value()->execute(token_var, var, var, var, var, var,
                                          var, var, var))) {
    return false;
  }

  // TODO(crbug.com/41377531): Get the status of the app command execution and
  // return a corresponding value for |success|. For now, assume that the call
  // to setup.exe succeeds.
  return true;
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool DeleteDMTokenFromRegistry() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto app_command = GetUpdaterAppCommand(installer::kCmdDeleteDMToken);
  if (!app_command.has_value()) {
    return false;
  }

  VARIANT var;
  VariantInit(&var);
  if (FAILED(app_command.value()->execute(var, var, var, var, var, var, var,
                                          var, var))) {
    return false;
  }

  // TODO(crbug.com/41377531): Get the status of the app command execution and
  // return a corresponding value for |success|. For now, assume that the call
  // to setup.exe succeeds.
  return true;
#else
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}
}  // namespace

std::string BrowserDMTokenStorageWin::InitClientId() {
  // For the client id, use the Windows machine GUID.
  base::win::RegKey key;
  LSTATUS status =
      key.Open(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography",
               KEY_QUERY_VALUE | KEY_WOW64_64KEY);
  if (status != ERROR_SUCCESS)
    return std::string();

  std::wstring value;
  status = key.ReadValue(L"MachineGuid", &value);
  if (status != ERROR_SUCCESS)
    return std::string();

  std::string client_id;
  if (!base::WideToUTF8(value.c_str(), value.length(), &client_id))
    return std::string();
  return client_id;
}

std::string BrowserDMTokenStorageWin::InitEnrollmentToken() {
  return base::WideToUTF8(InstallUtil::GetCloudManagementEnrollmentToken());
}

std::string BrowserDMTokenStorageWin::InitDMToken() {
  // At the time of writing (January 2018), the DM token is about 200 bytes
  // long. The initial size of the buffer should be enough to cover most
  // realistic future size-increase scenarios, although we still make an effort
  // to support somewhat larger token sizes just to be safe.
  constexpr size_t kInitialDMTokenSize = 512;

  base::win::RegKey key;
  std::wstring dm_token_value_name;
  std::vector<char> raw_value(kInitialDMTokenSize);

  // Prefer the app-neutral location over the browser's to match Google Update's
  // behavior.
  for (const auto& location : {InstallUtil::BrowserLocation(false),
                               InstallUtil::BrowserLocation(true)}) {
    std::tie(key, dm_token_value_name) =
        InstallUtil::GetCloudManagementDmTokenLocation(
            InstallUtil::ReadOnly(true), location);
    if (!key.Valid())
      continue;

    DWORD dtype = REG_NONE;
    DWORD size = static_cast<DWORD>(raw_value.size());
    auto result = key.ReadValue(dm_token_value_name.c_str(), raw_value.data(),
                                &size, &dtype);
    if (result == ERROR_MORE_DATA && size <= installer::kMaxDMTokenLength) {
      raw_value.resize(size);
      result = key.ReadValue(dm_token_value_name.c_str(), raw_value.data(),
                             &size, &dtype);
    }
    if (result != ERROR_SUCCESS || dtype != REG_BINARY || size == 0)
      continue;

    DCHECK_LE(size, installer::kMaxDMTokenLength);
    return std::string(base::TrimWhitespaceASCII(
        std::string_view(raw_value.data(), size), base::TRIM_ALL));
  }

  DVLOG(1) << "Failed to get DMToken from Registry.";
  return std::string();
}

bool BrowserDMTokenStorageWin::InitEnrollmentErrorOption() {
  return InstallUtil::ShouldCloudManagementBlockOnFailure();
}

bool BrowserDMTokenStorageWin::CanInitEnrollmentToken() const {
  return true;
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageWin::SaveDMTokenTask(
    const std::string& token,
    const std::string& client_id) {
  return base::BindOnce(&StoreDMTokenInRegistry, token);
}

BrowserDMTokenStorage::StoreTask BrowserDMTokenStorageWin::DeleteDMTokenTask(
    const std::string& client_id) {
  return base::BindOnce(&DeleteDMTokenFromRegistry);
}

scoped_refptr<base::TaskRunner>
BrowserDMTokenStorageWin::SaveDMTokenTaskRunner() {
  return com_sta_task_runner_;
}

BrowserDMTokenStorageWin::BrowserDMTokenStorageWin()
    : com_sta_task_runner_(
          base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()})) {}

BrowserDMTokenStorageWin::~BrowserDMTokenStorageWin() = default;

}  // namespace policy
