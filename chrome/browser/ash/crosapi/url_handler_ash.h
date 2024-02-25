// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_URL_HANDLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_URL_HANDLER_ASH_H_

#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// This class is the ash-chrome implementation of the UrlHandler interface.
// This class must only be used from the main thread.
class UrlHandlerAsh : public mojom::UrlHandler {
 public:
  UrlHandlerAsh();
  UrlHandlerAsh(const UrlHandlerAsh&) = delete;
  UrlHandlerAsh& operator=(const UrlHandlerAsh&) = delete;
  ~UrlHandlerAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::UrlHandler> receiver);

  // crosapi::mojom::UrlHandler:
  void OpenUrl(const GURL& url) override;
  void GetExternalHandler(const GURL& url,
                          GetExternalHandlerCallback callback) override;
  void OpenExternal(const GURL& url) override;

  // Returns |false| when the URL was invalid and will not get processed and
  // |true| when the URL will get processed (synchronous or asynchronously).
  // For testing only.
  bool OpenUrlInternal(GURL url);

 private:
  mojo::ReceiverSet<mojom::UrlHandler> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_URL_HANDLER_ASH_H_
