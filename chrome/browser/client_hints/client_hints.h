// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
#define CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_user_data.h"

class GURL;

namespace client_hints {

class ClientHints : public KeyedService,
                    public content::ClientHintsControllerDelegate,
                    public content::WebContentsUserData<ClientHints> {
 public:
  explicit ClientHints(content::BrowserContext* context);
  explicit ClientHints(content::WebContents* tab);
  ~ClientHints() override;

  // content::ClientHintsControllerDelegate:
  network::NetworkQualityTracker* GetNetworkQualityTracker() override;

  void GetAllowedClientHintsFromSource(
      const GURL& url,
      blink::WebEnabledClientHints* client_hints) override;

  bool IsJavaScriptAllowed(const GURL& url) override;

  std::string GetAcceptLanguageString() override;

  blink::UserAgentMetadata GetUserAgentMetadata() override;

  void PersistClientHints(
      const url::Origin& primary_origin,
      const std::vector<blink::mojom::WebClientHintsType>& client_hints,
      base::TimeDelta expiration_duration) override;

 private:
  content::BrowserContext* GetContext();

  friend class content::WebContentsUserData<ClientHints>;
  content::BrowserContext* context_ = nullptr;
  std::unique_ptr<
      content::WebContentsFrameBindingSet<client_hints::mojom::ClientHints>>
      binding_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ClientHints);
};

}  // namespace client_hints

#endif  // CHROME_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
