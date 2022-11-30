// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FEEDBACK_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FEEDBACK_ASH_H_

#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the crosapi feedback interface. Lives in ash-chrome on the
// UI thread. Shows feedback page in response to mojo IPCs from lacros-chrome.
class FeedbackAsh : public mojom::Feedback {
 public:
  FeedbackAsh();
  FeedbackAsh(const FeedbackAsh&) = delete;
  FeedbackAsh& operator=(const FeedbackAsh&) = delete;
  ~FeedbackAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Feedback> receiver);

  // crosapi::mojom::Feedback:
  void ShowFeedbackPage(mojom::FeedbackInfoPtr feedback_info) override;

 private:
  mojo::ReceiverSet<mojom::Feedback> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FEEDBACK_ASH_H_
