// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_TEST_MOCK_XHR_SENDER_H_
#define ASH_WEBUI_PROJECTOR_APP_TEST_MOCK_XHR_SENDER_H_

#include <string>

#include "ash/webui/projector_app/projector_xhr_sender.h"
#include "base/functional/callback.h"

namespace network::mojom {
class URLLoaderFactory;
}  // namespace network::mojom

class GURL;

namespace ash {

// A mock class of ProjectorXhrSender which helps to verify that the update
// indexable text request is sent correctly.
class MockXhrSender : public ProjectorXhrSender {
 public:
  using OnSendCallback =
      base::OnceCallback<void(const GURL&,
                              projector::mojom::RequestType,
                              const std::optional<std::string>&)>;

  MockXhrSender(OnSendCallback quit_closure,
                network::mojom::URLLoaderFactory* url_loader_factory);
  MockXhrSender(const MockXhrSender&) = delete;
  MockXhrSender& operator=(const MockXhrSender&) = delete;
  ~MockXhrSender() override;

  // ProjectorXhrSender:
  void Send(
      const GURL& url,
      projector::mojom::RequestType method,
      const std::optional<std::string>& request_body,
      bool use_credentials,
      bool use_api_key,
      SendRequestCallback callback,
      const std::optional<base::flat_map<std::string, std::string>>& headers,
      const std::optional<std::string>& account_email) override;

 private:
  // Quits the current run loop. Used to verify the MockXhrSender::Send getting
  // called.
  OnSendCallback quit_closure_;
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_TEST_MOCK_XHR_SENDER_H_
