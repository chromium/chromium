// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_URL_HANDLER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_URL_HANDLER_ASH_H_

#include <memory>
#include "chrome/browser/ui/ash/chrome_url_window_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_url_window_observer.h"
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

 private:
  mojo::ReceiverSet<mojom::UrlHandler> receivers_;

  // It is assumed that url_window_manager_ outlives url_window_observer_.
  std::unique_ptr<ChromeUrlWindowObserver> url_window_observer_;
  std::unique_ptr<ChromeUrlWindowManager> url_window_manager_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_URL_HANDLER_ASH_H_
