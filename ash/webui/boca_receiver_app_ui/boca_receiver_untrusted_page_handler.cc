// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_page_handler.h"

#include <utility>

#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
#include "chromeos/ash/components/boca/receiver/receiver_handler_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::boca_receiver {

BocaReceiverUntrustedPageHandler::BocaReceiverUntrustedPageHandler(
    mojo::PendingRemote<mojom::UntrustedPage> page,
    ReceiverHandlerDelegate* delegate)
    : page_(std::move(page)), delegate_(delegate) {}

BocaReceiverUntrustedPageHandler::~BocaReceiverUntrustedPageHandler() = default;

}  // namespace ash::boca_receiver
