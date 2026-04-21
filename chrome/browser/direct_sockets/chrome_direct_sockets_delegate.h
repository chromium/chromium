// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIRECT_SOCKETS_CHROME_DIRECT_SOCKETS_DELEGATE_H_
#define CHROME_BROWSER_DIRECT_SOCKETS_CHROME_DIRECT_SOCKETS_DELEGATE_H_

#include "content/public/browser/direct_sockets_delegate.h"

class ChromeDirectSocketsDelegate : public content::DirectSocketsDelegate {
 public:
  // content::DirectSocketsDelegate:
  bool AreDirectSocketsAllowed(content::BrowserContext* browser_context,
                               const url::Origin& origin) override;
  bool ValidateRequest(content::RenderFrameHost& rfh,
                       const RequestDetails&) override;
  bool ValidateRequestForSharedWorker(content::BrowserContext* browser_context,
                                      const GURL& shared_worker_url,
                                      const RequestDetails&) override;
  bool ValidateRequestForServiceWorker(content::BrowserContext* browser_context,
                                       const url::Origin& origin,
                                       const RequestDetails&) override;
};

#endif  // CHROME_BROWSER_DIRECT_SOCKETS_CHROME_DIRECT_SOCKETS_DELEGATE_H_
