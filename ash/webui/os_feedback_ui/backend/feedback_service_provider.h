// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_FEEDBACK_SERVICE_PROVIDER_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_FEEDBACK_SERVICE_PROVIDER_H_

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {
namespace feedback {

class FeedbackServiceProvider : os_feedback_ui::mojom::FeedbackServiceProvider {
 public:
  FeedbackServiceProvider();
  FeedbackServiceProvider(const FeedbackServiceProvider&) = delete;
  FeedbackServiceProvider& operator=(const FeedbackServiceProvider&) = delete;
  ~FeedbackServiceProvider() override;

  // os_feedback_ui::mojom::FeedbackServiceProvider:
  void GetFeedbackContext(GetFeedbackContextCallback callback) override;

  void BindInterface(
      mojo::PendingReceiver<os_feedback_ui::mojom::FeedbackServiceProvider>
          receiver);

 private:
  mojo::Receiver<os_feedback_ui::mojom::FeedbackServiceProvider> receiver_{
      this};
  base::WeakPtrFactory<FeedbackServiceProvider> weak_ptr_factory_{this};
};

}  // namespace feedback
}  // namespace ash

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_FEEDBACK_SERVICE_PROVIDER_H_
