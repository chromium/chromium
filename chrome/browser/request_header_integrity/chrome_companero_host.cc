// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/request_header_integrity/chrome_companero_host.h"

#include <utility>

#include "base/functional/callback.h"
#include "services/network/public/mojom/http_request_headers.mojom.h"

namespace request_header_integrity {

ChromeCompaneroHost::ChromeCompaneroHost() {
  // Foundational stub: background initialization task will be dispatched here.
}

ChromeCompaneroHost::~ChromeCompaneroHost() = default;

void ChromeCompaneroHost::BindReceiver(
    mojo::PendingReceiver<mojom::ChromeCompanero> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ChromeCompaneroHost::GetHeaderNameAndValue(
    GetHeaderNameAndValueCallback callback) {
  // Stub returns null. Full DSO evaluation implemented in follow-up CL.
  std::move(callback).Run(nullptr);
}

}  // namespace request_header_integrity
