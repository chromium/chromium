// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_SETUP_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_SETUP_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"

namespace ash {

enum class ASH_PUBLIC_EXPORT FlowType {
  // The whole consent flow.
  kConsentFlow,
  // The speaker id enrollment flow.
  kSpeakerIdEnrollment,
  // The speaker id retrain flow.
  kSpeakerIdRetrain,
};

// Interface for a class which is responsible for start Assistant OptIn flow.
class ASH_PUBLIC_EXPORT AssistantSetup {
 public:
  static AssistantSetup* GetInstance();

  AssistantSetup(const AssistantSetup&) = delete;
  AssistantSetup& operator=(const AssistantSetup&) = delete;

  using StartAssistantOptInFlowCallback = base::OnceCallback<void(bool)>;

  // Start the assistant setup flow.
  // |completed| is true if the user has completed the entire flow and opted in
  // to using assistant.
  virtual void StartAssistantOptInFlow(
      FlowType type,
      StartAssistantOptInFlowCallback on_completed) = 0;

  // Returns true and bounces the opt-in window if it is active.
  virtual bool BounceOptInWindowIfActive() = 0;

 protected:
  AssistantSetup();
  virtual ~AssistantSetup();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_SETUP_H_
