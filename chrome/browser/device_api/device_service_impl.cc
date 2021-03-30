// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/device_api/device_service_impl.h"

#include <memory>

#include "chrome/browser/device_api/device_attribute_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/origin.h"

namespace {

bool IsTrustedContext(content::RenderFrameHost* host,
                      const url::Origin& origin) {
  // TODO(anqing): This is used for dev trial. The flag will be removed when
  // permission policies are ready.
  if (!base::FeatureList::IsEnabled(features::kEnableRestrictedWebApis))
    return false;

  PrefService* prefs =
      Profile::FromBrowserContext(host->GetBrowserContext())->GetPrefs();

  // TODO(apotapchuk): Implement a more efficient way of checking the trustness
  // status of the app.
  for (const base::Value& entry :
       prefs->GetList(prefs::kWebAppInstallForceList)->GetList()) {
    if (origin ==
        url::Origin::Create(GURL(entry.FindKey(web_app::kUrlKey)->GetString())))
      return true;
  }
  return false;
}

}  // namespace

DeviceServiceImpl::DeviceServiceImpl(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver)
    : FrameServiceBase(host, std::move(receiver)), host_(host) {
  pref_change_registrar_.Init(
      Profile::FromBrowserContext(host->GetBrowserContext())->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kWebAppInstallForceList,
      base::BindRepeating(&DeviceServiceImpl::OnForceInstallWebAppListChanged,
                          base::Unretained(this)));
}

DeviceServiceImpl::~DeviceServiceImpl() = default;

// static
void DeviceServiceImpl::Create(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::DeviceAPIService> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!IsTrustedContext(host,
                        url::Origin::Create(host->GetLastCommittedURL()))) {
    // Not sending bad message here since the API is always exposed to the end
    // user.
    return;
  }
  // The object is bound to the lifetime of |host| and the mojo
  // connection. See FrameServiceBase for details.
  new DeviceServiceImpl(host, std::move(receiver));
}

void DeviceServiceImpl::OnForceInstallWebAppListChanged() {
  // DeviceServiceImpl is allocated on the heap, thus it is safe to remove it
  // like this.
  if (!IsTrustedContext(host_, origin())) {
    delete this;
  }
}

void DeviceServiceImpl::GetDirectoryId(GetDirectoryIdCallback callback) {
  device_attribute_api::GetDirectoryId(std::move(callback));
}

void DeviceServiceImpl::GetHostname(GetHostnameCallback callback) {
  device_attribute_api::GetHostname(std::move(callback));
}

void DeviceServiceImpl::GetSerialNumber(GetSerialNumberCallback callback) {
  device_attribute_api::GetSerialNumber(std::move(callback));
}

void DeviceServiceImpl::GetAnnotatedAssetId(
    GetAnnotatedAssetIdCallback callback) {
  device_attribute_api::GetAnnotatedAssetId(std::move(callback));
}

void DeviceServiceImpl::GetAnnotatedLocation(
    GetAnnotatedLocationCallback callback) {
  device_attribute_api::GetAnnotatedLocation(std::move(callback));
}
