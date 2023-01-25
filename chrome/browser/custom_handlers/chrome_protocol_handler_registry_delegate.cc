// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/custom_handlers/chrome_protocol_handler_registry_delegate.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/child_process_security_policy.h"

class PrefService;

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

ChromeProtocolHandlerRegistryDelegate::ChromeProtocolHandlerRegistryDelegate() =
    default;

ChromeProtocolHandlerRegistryDelegate::
    ~ChromeProtocolHandlerRegistryDelegate() = default;

// ProtocolHandlerRegistry::Delegate:
void ChromeProtocolHandlerRegistryDelegate::RegisterExternalHandler(
    const std::string& protocol) {
  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();
  if (!policy->IsWebSafeScheme(protocol)) {
    policy->RegisterWebSafeScheme(protocol);
  }
}

bool ChromeProtocolHandlerRegistryDelegate::IsExternalHandlerRegistered(
    const std::string& protocol) {
  // NOTE(koz): This function is safe to call from any thread, despite living
  // in ProfileIOData.
  return ProfileIOData::IsHandledProtocol(protocol);
}

void ChromeProtocolHandlerRegistryDelegate::RegisterWithOSAsDefaultClient(
    const std::string& protocol,
    DefaultClientCallback callback) {
  // The worker pointer is reference counted. While it is running, the
  // sequence it runs on will hold references it will be automatically freed
  // once all its tasks have finished.
  base::MakeRefCounted<shell_integration::DefaultSchemeClientWorker>(protocol)
      ->StartSetAsDefault(
          GetDefaultWebClientCallback(protocol, std::move(callback)));
}

void ChromeProtocolHandlerRegistryDelegate::CheckDefaultClientWithOS(
    const std::string& protocol,
    DefaultClientCallback callback) {
  // The worker pointer is reference counted. While it is running, the
  // sequence it runs on will hold references it will be automatically freed
  // once all its tasks have finished.
  base::MakeRefCounted<shell_integration::DefaultSchemeClientWorker>(protocol)
      ->StartCheckIsDefault(
          GetDefaultWebClientCallback(protocol, std::move(callback)));
}

// If true default protocol handlers will be removed if the OS level
// registration for a protocol is no longer Chrome.
bool ChromeProtocolHandlerRegistryDelegate::ShouldRemoveHandlersNotInOS() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // We don't do this on Linux as the OS registration there is not reliable,
  // and Chrome OS doesn't have any notion of OS registration.
  // TODO(benwells): When Linux support is more reliable remove this
  // difference (http://crbug.com/88255).
  return false;
#else
  return shell_integration::GetDefaultSchemeClientSetPermission() !=
         shell_integration::SET_DEFAULT_NOT_ALLOWED;
#endif
}

void ChromeProtocolHandlerRegistryDelegate::
    OnSetAsDefaultClientForSchemeFinished(
        const std::string& protocol,
        DefaultClientCallback callback,
        shell_integration::DefaultWebClientState state) {
  bool is_default = state != shell_integration::NOT_DEFAULT &&
                    state != shell_integration::OTHER_MODE_IS_DEFAULT;
  std::move(callback).Run(is_default);
}

shell_integration::DefaultWebClientWorkerCallback
ChromeProtocolHandlerRegistryDelegate::GetDefaultWebClientCallback(
    const std::string& protocol,
    DefaultClientCallback callback) {
  return base::BindOnce(&ChromeProtocolHandlerRegistryDelegate::
                            OnSetAsDefaultClientForSchemeFinished,
                        weak_ptr_factory_.GetWeakPtr(), protocol,
                        std::move(callback));
}
