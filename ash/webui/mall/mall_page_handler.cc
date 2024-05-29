// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/mall/mall_page_handler.h"

#include "chromeos/constants/url_constants.h"

namespace ash {

MallPageHandler::MallPageHandler(
    mojo::PendingReceiver<mall::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

MallPageHandler::~MallPageHandler() = default;

void MallPageHandler::GetMallEmbedUrl(GetMallEmbedUrlCallback callback) {
  // TODO(b/342057600): Use ash::GetMallLaunchUrl to generate a URL with the
  // context query parameter set.
  std::move(callback).Run(GURL(chromeos::kAppMallBaseUrl));
}

}  // namespace ash
