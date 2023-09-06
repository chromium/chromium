// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/device_api/device_service_impl.h"

#include <memory>

#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/device_api/device_attribute_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace {

// Check whether the target origin is allowed to access to the device
// attributes.
bool CanAccessDeviceAttributes(const PrefService* prefs,
                               const url::Origin& origin) {
  const base::Value::List& prefs_list =
      prefs->GetList(prefs::kDeviceAttributesAllowedForOrigins);

  return base::Contains(prefs_list, origin, [](const auto& entry) {
    return url::Origin::Create(GURL(entry.GetString()));
  });
}

// Check whether the target origin is the same as the main application running
// in the Kiosk session.
bool IsEqualToKioskOrigin(const url::Origin& origin) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  const ash::WebKioskAppData* app_data =
      ash::WebKioskAppManager::Get()->GetAppByAccountId(account_id);
  return url::Origin::Create(app_data->install_url()) == origin;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  DCHECK(KioskSessionServiceLacros::Get());
  return url::Origin::Create(
             KioskSessionServiceLacros::Get()->GetInstallURL()) == origin;
#else
  return false;
#endif
}

// Check whether the target origin is included in the WebAppInstallForceList
// policy.
bool IsForceInstalledOrigin(const PrefService* prefs,
                            const url::Origin& origin) {
  const base::Value::List& prefs_list =
      prefs->GetList(prefs::kWebAppInstallForceList);

  return base::Contains(prefs_list, origin, [](const auto& entry) {
    std::string entry_url =
        CHECK_DEREF(entry.GetDict().FindString(web_app::kUrlKey));
    return url::Origin::Create(GURL(entry_url));
  });
}

const Profile* GetProfile(content::RenderFrameHost& host) {
  return Profile::FromBrowserContext(host.GetBrowserContext());
}

const PrefService* GetPrefs(content::RenderFrameHost& host) {
  return GetProfile(host)->GetPrefs();
}

bool IsAffiliatedUser() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  return user && user->IsAffiliated();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return policy::PolicyLoaderLacros::IsMainUserAffiliated();
#else
  return false;
#endif
}

bool IsTrustedContext(content::RenderFrameHost& host,
                      const url::Origin& origin) {
  // Do not create the service for the incognito mode.
  if (GetProfile(host)->IsIncognitoProfile()) {
    return false;
  }

  if (chrome::IsRunningInAppMode()) {
    return IsEqualToKioskOrigin(origin);
  } else {
    return IsForceInstalledOrigin(GetPrefs(host), origin);
  }
}

}  // namespace

DeviceServiceImpl::DeviceServiceImpl(
    content::RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver)
    : DocumentService(host, std::move(receiver)) {
  pref_change_registrar_.Init(
      Profile::FromBrowserContext(host.GetBrowserContext())->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kDeviceAttributesAllowedForOrigins,
      base::BindRepeating(&DeviceServiceImpl::OnDisposingIfNeeded,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kWebAppInstallForceList,
      base::BindRepeating(&DeviceServiceImpl::OnDisposingIfNeeded,
                          base::Unretained(this)));
}

DeviceServiceImpl::~DeviceServiceImpl() = default;

// static
void DeviceServiceImpl::Create(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver) {
  CHECK(host);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsTrustedContext(*host,
                        host->GetMainFrame()->GetLastCommittedOrigin())) {
    // Not sending bad message here since the API is always exposed to the end
    // user.
    return;
  }
  // The object is bound to the lifetime of |host| and the mojo
  // connection. See DocumentService for details.
  new DeviceServiceImpl(*host, std::move(receiver));
}

// static
void DeviceServiceImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDeviceAttributesAllowedForOrigins);
}

void DeviceServiceImpl::OnDisposingIfNeeded() {
  // DeviceServiceImpl is allocated on the heap, thus it is safe to remove it
  // like this.
  if (!IsTrustedContext(render_frame_host(), origin())) {
    ResetAndDeleteThis();
  }
}

void DeviceServiceImpl::GetDirectoryId(GetDirectoryIdCallback callback) {
  GetDeviceAttribute(base::BindOnce(device_attribute_api::GetDirectoryId),
                     std::move(callback));
}

void DeviceServiceImpl::GetHostname(GetHostnameCallback callback) {
  GetDeviceAttribute(base::BindOnce(device_attribute_api::GetHostname),
                     std::move(callback));
}

void DeviceServiceImpl::GetSerialNumber(GetSerialNumberCallback callback) {
  GetDeviceAttribute(base::BindOnce(device_attribute_api::GetSerialNumber),
                     std::move(callback));
}

void DeviceServiceImpl::GetAnnotatedAssetId(
    GetAnnotatedAssetIdCallback callback) {
  GetDeviceAttribute(base::BindOnce(device_attribute_api::GetAnnotatedAssetId),
                     std::move(callback));
}

void DeviceServiceImpl::GetAnnotatedLocation(
    GetAnnotatedLocationCallback callback) {
  GetDeviceAttribute(base::BindOnce(device_attribute_api::GetAnnotatedLocation),
                     std::move(callback));
}

void DeviceServiceImpl::GetDeviceAttribute(
    base::OnceCallback<void(DeviceAttributeCallback)> handler,
    DeviceAttributeCallback callback) {
  if (!IsAffiliatedUser()) {
    device_attribute_api::ReportNotAffiliatedError(std::move(callback));
    return;
  }

  if (!CanAccessDeviceAttributes(GetPrefs(render_frame_host()), origin())) {
    device_attribute_api::ReportNotAllowedError(std::move(callback));
    return;
  }

  std::move(handler).Run(std::move(callback));
}
