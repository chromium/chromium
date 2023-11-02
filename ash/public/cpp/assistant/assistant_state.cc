// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_state.h"

#include <ostream>
#include <sstream>
#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
namespace {

AssistantState* g_assistant_state = nullptr;

}  // namespace

// static
AssistantState* AssistantState::Get() {
  return g_assistant_state;
}

AssistantState::AssistantState() {
  DCHECK(!g_assistant_state);
  g_assistant_state = this;
}

AssistantState::~AssistantState() {
  DCHECK_EQ(g_assistant_state, this);
  g_assistant_state = nullptr;
}

void AssistantState::NotifyStatusChanged(assistant::AssistantStatus status) {
  if (assistant_status_ == status)
    return;

  UpdateAssistantStatus(status);
}

void AssistantState::NotifyFeatureAllowed(
    assistant::AssistantAllowedState state) {
  if (allowed_state_ == state)
    return;

  UpdateFeatureAllowedState(state);
}

void AssistantState::NotifyLocaleChanged(const std::string& locale) {
  if (locale_ == locale)
    return;

  UpdateLocale(locale);
}

void AssistantState::NotifyArcPlayStoreEnabledChanged(bool enabled) {
  if (arc_play_store_enabled_ == enabled)
    return;

  UpdateArcPlayStoreEnabled(enabled);
}

void AssistantState::NotifyLockedFullScreenStateChanged(bool enabled) {
  if (locked_full_screen_enabled_ == enabled)
    return;

  UpdateLockedFullScreenState(enabled);
}

}  // namespace ash
