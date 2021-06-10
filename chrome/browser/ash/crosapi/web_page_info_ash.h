// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_WEB_PAGE_INFO_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_WEB_PAGE_INFO_ASH_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/crosapi/mojom/web_page_info.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the crosapi interface for web page info. Lives in Ash-Chrome on
// the UI thread.
class WebPageInfoFactoryAsh : public mojom::WebPageInfoFactory {
 public:
  WebPageInfoFactoryAsh();
  WebPageInfoFactoryAsh(const WebPageInfoFactoryAsh&) = delete;
  WebPageInfoFactoryAsh& operator=(const WebPageInfoFactoryAsh&) = delete;
  ~WebPageInfoFactoryAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::WebPageInfoFactory> receiver);

  // crosapi::mojom::WebPageInfoFactory:
  void RegisterWebPageInfoProvider(
      mojo::PendingRemote<mojom::WebPageInfoProvider> web_page_info_provider)
      override;

  // TODO(alanlxl): Add methods for smart dim to request lacros web page info.

 private:
  // Any number of crosapi clients can connect to this class.
  mojo::ReceiverSet<mojom::WebPageInfoFactory> receivers_;

  // This set maintains all registered web page info providers.
  mojo::RemoteSet<mojom::WebPageInfoProvider> web_page_info_providers_;

  base::WeakPtrFactory<WebPageInfoFactoryAsh> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_WEB_PAGE_INFO_ASH_H_
