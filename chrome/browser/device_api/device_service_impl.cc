// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/device_api/device_service_impl.h"

#include <functional>
#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/device_api/device_attribute_api.h"
#include "chrome/browser/permissions/permission_manager_factory.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller_delegate.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/kiosk_web_app_manager.h"
#include "chrome/common/url_constants.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
#if BUILDFLAG(IS_CHROMEOS)

// Returns an origin of the current kiosk web app.
// Should only be called when the current user is a web kiosk.
url::Origin GetWebKioskOrigin() {
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  CHECK(ash::KioskWebAppManager::IsInitialized());
  const ash::KioskWebAppData* app_data =
      ash::KioskWebAppManager::Get()->GetAppByAccountId(account_id);
  return url::Origin::Create(CHECK_DEREF(app_data).install_url());
}

// Returns an origin of the current kiosk isolated web app.
// Should only be called when the current user is an IWA kiosk.
url::Origin GetIwaKioskOrigin() {
  const AccountId& account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  const ash::KioskIwaData* iwa_data =
      CHECK_DEREF(ash::KioskIwaManager::Get()).GetApp(account_id);
  return CHECK_DEREF(iwa_data).origin();
}

std::optional<url::Origin> MaybeGetCurrentKioskOrigin() {
  if (chromeos::IsWebKioskSession()) {
    return GetWebKioskOrigin();
  }
  if (chromeos::IsIwaKioskSession()) {
    return GetIwaKioskOrigin();
  }
  return std::nullopt;
}

// Check whether the target origin is the same as the main application running
// in the Kiosk session.
bool IsEqualToKioskOrigin(const url::Origin& origin) {
  std::optional<url::Origin> current_kiosk_origin =
      MaybeGetCurrentKioskOrigin();
  return current_kiosk_origin.has_value() &&
         (current_kiosk_origin.value() == origin);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

Profile* GetProfile(content::RenderFrameHost& host) {
  return Profile::FromBrowserContext(host.GetBrowserContext());
}

std::optional<std::reference_wrapper<const web_app::WebAppRegistrar>>
GetRegistrar(content::RenderFrameHost& host, const url::Origin& origin) {
  const web_app::WebAppProvider* web_app_provider =
      web_app::WebAppProvider::GetForWebApps(GetProfile(host));
  if (!web_app_provider) {
    return std::nullopt;
  }
  // In this case we will not modify any data so it is safe to access registrar
  // without lock
  return web_app_provider->registrar_unsafe();
}

std::optional<webapps::AppId> GetAppId(
    const web_app::WebAppRegistrar& registrar,
    const url::Origin& origin) {
  return registrar.FindBestAppWithUrlInScope(
      origin.GetURL(), web_app::WebAppFilter::InstalledInChrome());
}

// Check whether an app with the target origin is in the WebAppRegistrar and is
// an IWA.
bool IsForceInstalledIwaOrigin(content::RenderFrameHost& host,
                               const url::Origin& origin) {
  ASSIGN_OR_RETURN(const web_app::WebAppRegistrar& registrar,
                   GetRegistrar(host, origin), [] { return false; });
  ASSIGN_OR_RETURN(webapps::AppId app_id, GetAppId(registrar, origin),
                   [] { return false; });
  return registrar.AppMatches(
      app_id, web_app::WebAppFilter::PolicyInstalledIsolatedWebApp());
}

// Check whether an app with the target origin is in the WebAppRegistrar.
bool IsForceInstalledOrigin(content::RenderFrameHost& host,
                            const url::Origin& origin) {
  ASSIGN_OR_RETURN(const web_app::WebAppRegistrar& registrar,
                   GetRegistrar(host, origin), [] { return false; });
  ASSIGN_OR_RETURN(webapps::AppId app_id, GetAppId(registrar, origin),
                   [] { return false; });
  return registrar.IsInstalledByPolicy(app_id);
}

bool IsAffiliatedUser() {
#if BUILDFLAG(IS_CHROMEOS)
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  return (user != nullptr) && user->IsAffiliated();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool IsPermissionsPolicyFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      blink::features::kDeviceAttributesPermissionPolicy);
}

bool IsTrustedContext(content::RenderFrameHost& host,
                      const url::Origin& origin) {
  // Do not create the service for the incognito mode.
  if (GetProfile(host)->IsIncognitoProfile()) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (IsRunningInAppMode()) {
    if (base::FeatureList::IsEnabled(
            permissions::features::
                kAllowMultipleOriginsForWebKioskPermissions)) {
      return IsEqualToKioskOrigin(origin) ||
             IsWebKioskOriginAllowed(GetProfile(host)->GetPrefs(),
                                     origin.GetURL());
    }

    return IsEqualToKioskOrigin(origin);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return IsPermissionsPolicyFeatureEnabled()
             ? IsForceInstalledIwaOrigin(host, origin)
             : IsForceInstalledOrigin(host, origin);
}

bool IsAllowedByPermissionsPolicy(content::RenderFrameHost& host) {
  return host.IsFeatureEnabled(
      network::mojom::PermissionsPolicyFeature::kDeviceAttributes);
}

bool IsAllowedByAdminPolicy(content::RenderFrameHost& host,
                            const url::Origin& origin) {
#if BUILDFLAG(IS_CHROMEOS)
  return policy::IsOriginInAllowlist(
      origin.GetURL(), GetProfile(host)->GetPrefs(),
      prefs::kManagedDeviceAttributesAllowedForOrigins);
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

bool IsAllowedByContentSettings(content::RenderFrameHost& host,
                                const url::Origin& origin) {
  switch (HostContentSettingsMapFactory::GetForProfile(GetProfile(host))
              ->GetContentSetting(origin.GetURL(), origin.GetURL(),
                                  ContentSettingsType::DEVICE_ATTRIBUTES)) {
    case CONTENT_SETTING_ALLOW:
      return true;
    case CONTENT_SETTING_BLOCK:
      return false;
    default:
      NOTREACHED();
  }
}
}  // namespace

DeviceServiceImpl::DeviceServiceImpl(
    content::RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver,
    std::unique_ptr<DeviceAttributeApi> device_attribute_api)
    : DocumentService(host, std::move(receiver)),
      device_attribute_api_(std::move(device_attribute_api)) {
  Profile* const profile = GetProfile(host);
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kWebAppInstallForceList,
      base::BindRepeating(&DeviceServiceImpl::OnDisposingIfNeeded,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kIsolatedWebAppInstallForceList,
      base::BindRepeating(&DeviceServiceImpl::OnDisposingIfNeeded,
                          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS)
  pref_change_registrar_.Add(
      prefs::kKioskBrowserPermissionsAllowedForOrigins,
      base::BindRepeating(&DeviceServiceImpl::OnDisposingIfNeeded,
                          base::Unretained(this)));
  if (!IsPermissionsPolicyFeatureEnabled()) {
    pref_change_registrar_.Add(
        prefs::kManagedDeviceAttributesAllowedForOrigins,
        base::BindRepeating(&DeviceServiceImpl::OnDisposingIfNeeded,
                            base::Unretained(this)));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  content_settings_observation_.Observe(
      HostContentSettingsMapFactory::GetForProfile(profile));
  auto& provider = CHECK_DEREF(web_app::WebAppProvider::GetForWebApps(profile));
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

  if (host->GetParentOrOuterDocumentOrEmbedder()) {
    mojo::ReportBadMessage(
        "Device Attributes are allowed only in top level frames.");
    return;
  }
  if (!IsTrustedContext(*host, host->GetLastCommittedOrigin())) {
    // Not sending bad message here since the API is always exposed to the end
    // user.
    return;
  }
  if (IsPermissionsPolicyFeatureEnabled() &&
      !IsAllowedByPermissionsPolicy(*host)) {
    mojo::ReportBadMessage(
        "Permissions policy blocks access to Device Attributes.");
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

void DeviceServiceImpl::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (!content_type_set.Contains(ContentSettingsType::DEVICE_ATTRIBUTES)) {
    return;
  }
  OnDisposingIfNeeded();
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

  if (IsPermissionsPolicyFeatureEnabled()) {
    if (!IsAllowedByContentSettings(render_frame_host(), origin())) {
      device_attribute_api_->ReportNotAllowedError(std::move(callback));
      return;
    }
  } else {
    if (!IsAllowedByAdminPolicy(render_frame_host(), origin())) {
      device_attribute_api_->ReportNotAllowedError(std::move(callback));
      return;
    }
  }

  (device_attribute_api_.get()->*method)(std::move(callback));
}
