// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_PAGE_HANDLER_H_
#define ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_PAGE_HANDLER_H_

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::boca_receiver {

class ReceiverHandlerDelegate;

class BocaReceiverUntrustedPageHandler {
 public:
  BocaReceiverUntrustedPageHandler(
      mojo::PendingRemote<mojom::UntrustedPage> page,
      ReceiverHandlerDelegate* delegate);

  BocaReceiverUntrustedPageHandler(const BocaReceiverUntrustedPageHandler&) =
      delete;
  BocaReceiverUntrustedPageHandler& operator=(
      const BocaReceiverUntrustedPageHandler&) = delete;

  ~BocaReceiverUntrustedPageHandler();

 private:
  mojo::Remote<mojom::UntrustedPage> page_;
  raw_ptr<ReceiverHandlerDelegate> delegate_;
};

}  // namespace ash::boca_receiver

#endif  // ASH_WEBUI_BOCA_RECEIVER_APP_UI_BOCA_RECEIVER_UNTRUSTED_PAGE_HANDLER_H_
