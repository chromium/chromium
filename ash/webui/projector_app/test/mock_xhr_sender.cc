// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/test/mock_xhr_sender.h"

#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash {
MockXhrSender::MockXhrSender(
    OnSendCallback quit_closure,
    network::mojom::URLLoaderFactory* url_loader_factory)
    : ProjectorXhrSender(url_loader_factory),
      quit_closure_(std::move(quit_closure)) {}

MockXhrSender::~MockXhrSender() = default;

void MockXhrSender::Send(
    const GURL& url,
    projector::mojom::RequestType method,
    const std::optional<std::string>& request_body,
    bool use_credentials,
    bool use_api_key,
    SendRequestCallback callback,
    const std::optional<base::flat_map<std::string, std::string>>& headers,
    const std::optional<std::string>& account_email) {
  std::move(quit_closure_).Run(url, method, request_body);
}
}  // namespace ash
