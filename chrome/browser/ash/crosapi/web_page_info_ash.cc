// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/web_page_info_ash.h"

namespace crosapi {

WebPageInfoFactoryAsh::WebPageInfoFactoryAsh() = default;

WebPageInfoFactoryAsh::~WebPageInfoFactoryAsh() = default;

void WebPageInfoFactoryAsh::BindReceiver(
    mojo::PendingReceiver<mojom::WebPageInfoFactory> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WebPageInfoFactoryAsh::RegisterWebPageInfoProvider(
    mojo::PendingRemote<mojom::WebPageInfoProvider> web_page_info_provider) {
  mojo::Remote<mojom::WebPageInfoProvider> remote(
      std::move(web_page_info_provider));
  web_page_info_providers_.Add(std::move(remote));
}

}  // namespace crosapi
