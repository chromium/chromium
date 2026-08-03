// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_HANDLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_HANDLER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace glic {
class Host;

using WebClientStateChangedCallback =
    base::RepeatingCallback<void(mojom::WebClientState state)>;

std::unique_ptr<GlicWebClientAccess> MakeGlicWebClient(
    Host* host,
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver,
    base::OnceClosure disconnect_callback,
    WebClientStateChangedCallback state_changed_callback);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_HANDLER_H_
