// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/web_page_info_lacros.h"

#include "chromeos/lacros/lacros_service.h"

namespace crosapi {

WebPageInfoProviderLacros::WebPageInfoProviderLacros() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<mojom::WebPageInfoFactory>())
    return;
  service->GetRemote<mojom::WebPageInfoFactory>()->RegisterWebPageInfoProvider(
      receiver_.BindNewPipeAndPassRemote());
}

WebPageInfoProviderLacros::~WebPageInfoProviderLacros() = default;

void WebPageInfoProviderLacros::RequestCurrentWebPageInfo(
    RequestCurrentWebPageInfoCallback callback) {
  // TODO(alanlxl): fetch the real web page info.
  std::move(callback).Run(nullptr);
}

}  // namespace crosapi
