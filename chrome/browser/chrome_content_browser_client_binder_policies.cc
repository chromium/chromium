// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_binder_policies.h"

#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"

void RegisterChromeMojoBinderPoliciesForSameOriginPrerendering(
    content::MojoBinderPolicyMap& policy_map) {
  // TODO(https://crbug.com/1145976): Set all Chrome's interface policies.

  // Prerendering does not happen for WebUI pages, so set kUnexpected as the
  // policy for interfaces registered by WebUI.
  policy_map.SetPolicy<::mojom::BluetoothInternalsHandler>(
      content::MojoBinderPolicy::kUnexpected);
}
