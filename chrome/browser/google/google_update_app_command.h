// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_APP_COMMAND_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_APP_COMMAND_H_

#include <windows.h>

#include <wrl/client.h>

#include <string>
#include <utility>

#include "base/types/expected.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"

// Explicitly allows the Google Update service to impersonate the client since
// some COM code elsewhere in the browser process may have previously used
// CoInitializeSecurity to set the impersonation level to something other than
// the default.
// Ignores errors since an attempt to use Google Update may succeed
// regardless.
void ConfigureProxyBlanket(IUnknown* interface_pointer);

// Gets the AppCommand with the given name, ready to be invoked by the Google
// Updater.
base::expected<Microsoft::WRL::ComPtr<IAppCommandWeb>, HRESULT>
GetUpdaterAppCommand(const std::wstring& command_name);

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_UPDATE_APP_COMMAND_H_
