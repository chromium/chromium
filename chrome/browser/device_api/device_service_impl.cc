// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/device_api/device_service_impl.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/device_api/device_attribute_api.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/proto/proto_helpers.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_proto_package.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/common/url_constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace {

// Check whether the target origin is the same as the main application running
// in the Kiosk session.
bool IsEqualToKioskOrigin(const url::Origin& origin) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  const ash::WebKioskAppData* app_data =
      ash::WebKioskAppManager::Get()->GetAppByAccountId(account_id);
  if (!app_data) {
    // This can happen when the device service APIs are accessed from inside a
    // ChromeApp.
    return false;
  }

  return url::Origin::Create(app_data->install_url()) == origin;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  DCHECK(KioskSessionServiceLacros::Get());
  return url::Origin::Create(
             KioskSessionServiceLacros::Get()->GetInstallURL()) == origin;
#else
  return false;
#endif
}

Profile* GetProfile(content::RenderFrameHost& host) {
  return Profile::FromBrowserContext(host.GetBrowserContext());
}

// Check whether an app with the target origin is in the WebAppRegistrar.
bool IsForceInstalledOrigin(content::RenderFrameHost& host,
                            const url::Origin& origin) {
  web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForWebApps(GetProfile(host));

  if (!web_app_provider) {
    return false;
  }

  // In this case we will not modify any data so it is safe to access
  // registrar without lock
  const web_app::WebAppRegistrar& registrar =
      web_app_provider->registrar_unsafe();

  const auto app_id = registrar.FindBestAppWithUrlInScope(
      origin.GetURL(),
      {
          web_app::proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
          web_app::proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
      },
      {.include_extended_scope = true});

  if (!app_id.has_value()) {
    return false;
  }

  return registrar.IsInstalledByPolicy(app_id.value());
}

const PrefService* GetPrefs(content::RenderFrameHost& host) {
  return GetProfile(host)->GetPrefs();
}

bool IsAffiliatedUser() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  return (user != nullptr) && user->IsAffiliated();
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

  if (IsRunningInAppMode()) {
    if (base::FeatureList::IsEnabled(
            permissions::features::
                kAllowMultipleOriginsForWebKioskPermissions)) {
      return IsEqualToKioskOrigin(origin) ||
             IsWebKioskOriginAllowed(GetPrefs(host), origin.GetURL());
    }

    return IsEqualToKioskOrigin(origin);
  }
  return IsForceInstalledOrigin(host, origin);
}

}  // namespace

DeviceServiceImpl::DeviceServiceImpl(
    content::RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
    std::unique_ptr<DeviceAttributeApi> device_attribute_api)
    : DocumentService(host, std::move(receiver)),
      device_attribute_api_(std::move(device_attribute_api)) {
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
#if BUILDFLAG(IS_CHROMEOS)
  pref_change_registrar_.Add(
      prefs::kIsolatedWebAppInstallForceList,
      base::BindRepeating(&DeviceServiceImpl::OnDisposingIfNeeded,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kKioskBrowserPermissionsAllowedForOrigins,
      base::BindRepeating(&DeviceServiceImpl::OnDisposingIfNeeded,
                          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS)
  auto& provider =
      CHECK_DEREF(web_app::WebAppProvider::GetForWebApps(GetProfile(host)));
  install_manager_observation_.Observe(&provider.install_manager());
}

DeviceServiceImpl::~DeviceServiceImpl() = default;

// static
void DeviceServiceImpl::Create(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
    std::unique_ptr<DeviceAttributeApi> device_attribute_api) {
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
  new DeviceServiceImpl(*host, std::move(receiver),
                        std::move(device_attribute_api));
}

// static
void DeviceServiceImpl::Create(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver) {
  Create(host, std::move(receiver), std::make_unique<DeviceAttributeApiImpl>());
}

// static
void DeviceServiceImpl::CreateForTest(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
    std::unique_ptr<DeviceAttributeApi> device_attribute_api) {
  CHECK_IS_TEST();
  Create(host, std::move(receiver), std::move(device_attribute_api));
}

// static
void DeviceServiceImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDeviceAttributesAllowedForOrigins);
}

void DeviceServiceImpl::OnWebAppSourceRemoved(const webapps::AppId& app_id) {
  OnDisposingIfNeeded();
}

void DeviceServiceImpl::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  OnDisposingIfNeeded();
}

void DeviceServiceImpl::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void DeviceServiceImpl::OnDisposingIfNeeded() {
  // DeviceServiceImpl is allocated on the heap, thus it is safe to remove it
  // like this.
  if (!IsTrustedContext(render_frame_host(), origin())) {
    ResetAndDeleteThis();
  }
}

void DeviceServiceImpl::GetDirectoryId(GetDirectoryIdCallback callback) {
  GetDeviceAttribute(&DeviceAttributeApi::GetDirectoryId, std::move(callback));
}

void DeviceServiceImpl::GetHostname(GetHostnameCallback callback) {
  GetDeviceAttribute(&DeviceAttributeApi::GetHostname, std::move(callback));
}

void DeviceServiceImpl::GetSerialNumber(GetSerialNumberCallback callback) {
  GetDeviceAttribute(&DeviceAttributeApi::GetSerialNumber, std::move(callback));
}

void DeviceServiceImpl::GetAnnotatedAssetId(
    GetAnnotatedAssetIdCallback callback) {
  GetDeviceAttribute(&DeviceAttributeApi::GetAnnotatedAssetId,
                     std::move(callback));
}

void DeviceServiceImpl::GetAnnotatedLocation(
    GetAnnotatedLocationCallback callback) {
  GetDeviceAttribute(&DeviceAttributeApi::GetAnnotatedLocation,
                     std::move(callback));
}

void DeviceServiceImpl::GetDeviceAttribute(
    void (DeviceAttributeApi::*method)(DeviceAttributeCallback callback),
    DeviceAttributeCallback callback) {
  if (!IsAffiliatedUser()) {
    device_attribute_api_->ReportNotAffiliatedError(std::move(callback));
    return;
  }

  if (!policy::IsOriginInAllowlist(origin().GetURL(),
                                   GetPrefs(render_frame_host()),
                                   prefs::kDeviceAttributesAllowedForOrigins)) {
    device_attribute_api_->ReportNotAllowedError(std::move(callback));
    return;
  }

  (device_attribute_api_.get()->*method)(std::move(callback));
}
