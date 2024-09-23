// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_api/managed_configuration_service.h"

#include <utility>

#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/features_generated.h"

// static
ManagedConfigurationServiceImpl* ManagedConfigurationServiceImpl::Create(
    content::RenderFrameHost* host,
    mojo::PendingReceiver<blink::mojom::ManagedConfigurationService> receiver) {
  CHECK(host);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(blink::features::kManagedConfiguration)) {
    mojo::ReportBadMessage(
        "Managed configuration access while the feature is not enabled.");
    return nullptr;
  }

  // Do not create ManagedConfigurationService for incognito or off-the-record
  // profiles.
  if (host->GetBrowserContext()->IsOffTheRecord() ||
      Profile::FromBrowserContext(host->GetBrowserContext())
          ->IsIncognitoProfile()) {
    return nullptr;
  }

  // The object is bound to the lifetime of |host| and the mojo
  // connection. See DocumentService for details.
  return new ManagedConfigurationServiceImpl(*host, std::move(receiver));
}

ManagedConfigurationServiceImpl::ManagedConfigurationServiceImpl(
    content::RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::ManagedConfigurationService> receiver)
    : DocumentService(host, std::move(receiver)) {
  managed_configuration_api()->AddObserver(this);
}

ManagedConfigurationServiceImpl::~ManagedConfigurationServiceImpl() {
  managed_configuration_api()->RemoveObserver(this);
}

void ManagedConfigurationServiceImpl::GetManagedConfiguration(
    const std::vector<std::string>& keys,
    GetManagedConfigurationCallback callback) {
  managed_configuration_api()->GetOriginPolicyConfiguration(
      origin(), keys,
      base::BindOnce(
          [](GetManagedConfigurationCallback callback,
             std::optional<base::Value::Dict> result) {
            if (!result) {
              std::move(callback).Run(std::nullopt);
              return;
            }
            std::move(callback).Run(base::MakeFlatMap<std::string, std::string>(
                *result, {},
                [](const auto& item) -> std::pair<std::string, std::string> {
                  return {item.first, item.second.GetString()};
                }));
          },
          std::move(callback)));
}

void ManagedConfigurationServiceImpl::SubscribeToManagedConfiguration(
    mojo::PendingRemote<blink::mojom::ManagedConfigurationObserver> observer) {
  CHECK(!configuration_subscription_.is_bound());
  configuration_subscription_.Bind(std::move(observer));
  configuration_subscription_.reset_on_disconnect();
}

void ManagedConfigurationServiceImpl::OnManagedConfigurationChanged() {
  if (configuration_subscription_.is_bound()) {
    configuration_subscription_->OnConfigurationChanged();
  }
}

ManagedConfigurationAPI*
ManagedConfigurationServiceImpl::managed_configuration_api() {
  return ManagedConfigurationAPIFactory::GetForProfile(
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext()));
}

const url::Origin& ManagedConfigurationServiceImpl::GetOrigin() const {
  return origin();
}
