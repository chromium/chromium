// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_RECEIVER_RECEIVER_HANDLER_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_RECEIVER_RECEIVER_HANDLER_DELEGATE_IMPL_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/boca/receiver/receiver_handler_delegate.h"

namespace ash::boca {
class FCMHandler;
class SpotlightRemotingClientManager;
}  // namespace ash::boca

namespace content {
class WebUI;
}  // namespace content

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash::boca_receiver {

class ReceiverHandlerDelegateImpl : public ReceiverHandlerDelegate {
 public:
  explicit ReceiverHandlerDelegateImpl(content::WebUI* web_ui);

  ReceiverHandlerDelegateImpl(const ReceiverHandlerDelegateImpl&) = delete;
  ReceiverHandlerDelegateImpl& operator=(const ReceiverHandlerDelegateImpl&) =
      delete;

  ~ReceiverHandlerDelegateImpl() override;

  // ReceiverHandlerDelegate:
  boca::FCMHandler* GetFcmHandler() const override;

  std::unique_ptr<google_apis::RequestSender> CreateRequestSender(
      std::string_view requester_id,
      const net::NetworkTrafficAnnotationTag& traffic_annotation)
      const override;

  boca::SpotlightRemotingClientManager* GetRemotingClient() const override;

  bool IsAppEnabled(std::string_view url) override;

 private:
  raw_ptr<content::WebUI> web_ui_;
};

}  // namespace ash::boca_receiver

#endif  // CHROME_BROWSER_ASH_BOCA_RECEIVER_RECEIVER_HANDLER_DELEGATE_IMPL_H_
