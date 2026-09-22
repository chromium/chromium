// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_cors_exempt_headers.h"

#include "services/network/public/mojom/network_context.mojom.h"

namespace glic {

void UpdateCorsExemptHeaders(network::mojom::NetworkContextParams* params) {
  params->cors_exempt_header_list.push_back(kGlicHeaderName);
  params->cors_exempt_header_list.push_back(kGlicVersionHeaderName);
  params->cors_exempt_header_list.push_back(kGlicChannelHeaderName);
}

}  // namespace glic
