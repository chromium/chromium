// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_interface_registration.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/media/router/media_router_feature.h"  // nogncheck
#include "chrome/browser/media/router/mojo/media_router_desktop.h"  // nogncheck
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/service_manager/public/cpp/binder_registry.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_features.h"
#include "chromeos/services/ime/public/mojom/constants.mojom.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/services/media_perception/public/mojom/media_perception.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/media_perception_private/media_perception_api_delegate.h"
#include "services/service_manager/public/cpp/connector.h"
#endif

namespace extensions {
namespace {
#if defined(OS_CHROMEOS)
// Forwards service requests to Service Manager since the renderer cannot launch
// out-of-process services on its own.
template <typename Interface>
void ForwardRequest(const char* service_name,
                    mojo::InterfaceRequest<Interface> request,
                    content::RenderFrameHost* source) {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(service_name, std::move(request));
}
#endif
}  // namespace

void RegisterChromeInterfacesForExtension(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);
  content::BrowserContext* context =
      render_frame_host->GetProcess()->GetBrowserContext();
  if (media_router::MediaRouterEnabled(context) &&
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kMediaRouterPrivate)) {
    registry->AddInterface(
        base::Bind(&media_router::MediaRouterDesktop::BindToRequest,
                   base::RetainedRef(extension), context));
  }

#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(
          chromeos::features::kImeServiceConnectable) &&
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kInputMethodPrivate)) {
    registry->AddInterface(base::BindRepeating(
        &ForwardRequest<chromeos::ime::mojom::InputEngineManager>,
        chromeos::ime::mojom::kServiceName));
  }

  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kMediaPerceptionPrivate)) {
    extensions::ExtensionsAPIClient* client =
        extensions::ExtensionsAPIClient::Get();
    extensions::MediaPerceptionAPIDelegate* delegate = nullptr;
    if (client)
      delegate = client->GetMediaPerceptionAPIDelegate();
    if (delegate) {
      // Note that it is safe to use base::Unretained here because |delegate| is
      // owned by the |client|, which is instantiated by the
      // ChromeExtensionsBrowserClient, which in turn is owned and lives as long
      // as the BrowserProcessImpl.
      registry->AddInterface(
          base::BindRepeating(&extensions::MediaPerceptionAPIDelegate::
                                  ForwardMediaPerceptionRequest,
                              base::Unretained(delegate)));
    }
  }
#endif
}

}  // namespace extensions
