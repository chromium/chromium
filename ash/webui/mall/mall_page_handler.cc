// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/mall/mall_page_handler.h"

#include "ash/webui/mall/mall_ui_delegate.h"
#include "chromeos/constants/url_constants.h"

namespace ash {

MallPageHandler::MallPageHandler(
    mojo::PendingReceiver<mall::mojom::PageHandler> receiver,
    MallUIDelegate& delegate)
    : receiver_(this, std::move(receiver)), delegate_(delegate) {}

MallPageHandler::~MallPageHandler() = default;

void MallPageHandler::GetMallEmbedUrl(const std::string& path,
                                      GetMallEmbedUrlCallback callback) {
  delegate_.get().GetMallEmbedUrl(path, std::move(callback));
}

}  // namespace ash
