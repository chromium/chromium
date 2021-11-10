// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace ash {

enum class ScreenContextRequestState;

// A checked observer which receives notification of changes to the Assistant
// screen context model state.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantScreenContextModelObserver
    : public base::CheckedObserver {
 public:
  AssistantScreenContextModelObserver(
      const AssistantScreenContextModelObserver&) = delete;
  AssistantScreenContextModelObserver& operator=(
      const AssistantScreenContextModelObserver&) = delete;

  // Invoked when the screen context request state is changed.
  virtual void OnScreenContextRequestStateChanged(
      ScreenContextRequestState request_state) {}

 protected:
  AssistantScreenContextModelObserver() = default;
  ~AssistantScreenContextModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_SCREEN_CONTEXT_MODEL_OBSERVER_H_
