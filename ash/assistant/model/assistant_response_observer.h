// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_OBSERVER_H_

#include "base/macros.h"

namespace ash {

class AssistantResponse;

// An observer which receives notification of changes to an Assistant response.
class AssistantResponseObserver {
 public:
  // Invoked when the specified |response| is being destroyed.
  virtual void OnResponseDestroying(AssistantResponse& response) {}

 protected:
  AssistantResponseObserver() = default;
  virtual ~AssistantResponseObserver() = default;

  DISALLOW_COPY_AND_ASSIGN(AssistantResponseObserver);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_OBSERVER_H_
