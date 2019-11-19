// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_state.h"

#include <ostream>
#include <sstream>
#include <utility>

#include "ash/public/mojom/assistant_state_controller.mojom.h"
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

void AssistantState::BindReceiver(
    mojo::PendingReceiver<mojom::AssistantStateController> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AssistantState::NotifyStatusChanged(mojom::AssistantState state) {
  if (assistant_state_ == state)
    return;

  UpdateAssistantStatus(state);
  for (auto& observer : remote_observers_)
    observer->OnAssistantStatusChanged(state);
}

void AssistantState::NotifyFeatureAllowed(mojom::AssistantAllowedState state) {
  if (allowed_state_ == state)
    return;

  UpdateFeatureAllowedState(state);
  for (auto& observer : remote_observers_)
    observer->OnAssistantFeatureAllowedChanged(state);
}

void AssistantState::NotifyLocaleChanged(const std::string& locale) {
  if (locale_ == locale)
    return;

  UpdateLocale(locale);
  for (auto& observer : remote_observers_)
    observer->OnLocaleChanged(locale);
}

void AssistantState::NotifyArcPlayStoreEnabledChanged(bool enabled) {
  if (arc_play_store_enabled_ == enabled)
    return;

  UpdateArcPlayStoreEnabled(enabled);
  for (auto& observer : remote_observers_)
    observer->OnArcPlayStoreEnabledChanged(enabled);
}

void AssistantState::NotifyLockedFullScreenStateChanged(bool enabled) {
  if (locked_full_screen_enabled_ == enabled)
    return;

  UpdateLockedFullScreenState(enabled);
  for (auto& observer : remote_observers_)
    observer->OnLockedFullScreenStateChanged(enabled);
}

void AssistantState::AddMojomObserver(
    mojo::PendingRemote<mojom::AssistantStateObserver> pending_observer) {
  auto remote =
      mojo::Remote<mojom::AssistantStateObserver>(std::move(pending_observer));
  mojom::AssistantStateObserver* observer = remote.get();
  remote_observers_.Add(std::move(remote));
  InitializeObserverMojom(observer);
}

}  // namespace ash
