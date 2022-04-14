// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/feedback_service_provider.h"

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

using ::ash::os_feedback_ui::mojom::FeedbackContext;
using ::ash::os_feedback_ui::mojom::FeedbackContextPtr;

void FeedbackServiceProvider::GetFeedbackContext(
    GetFeedbackContextCallback callback) {
  // TODO(xiangdongkong): Replace with real values.
  FeedbackContextPtr feedback_context =
      FeedbackContext::New("test@test.com", GURL("chrome://flags/"));
  std::move(callback).Run(std::move(feedback_context));
}

void FeedbackServiceProvider::BindInterface(
    mojo::PendingReceiver<os_feedback_ui::mojom::FeedbackServiceProvider>
        receiver) {
  receiver_.Bind(std::move(receiver));
}

FeedbackServiceProvider::FeedbackServiceProvider() = default;
FeedbackServiceProvider::~FeedbackServiceProvider() = default;

}  // namespace feedback
}  // namespace ash
