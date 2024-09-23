// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/backend/shortcut_input_provider.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/shortcut_input_handler.h"
#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/webui/common/mojom/shortcut_input_provider.mojom.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/widget/widget.h"

namespace ash {

ShortcutInputProvider::ShortcutInputProvider() {
  if (Shell::HasInstance()) {
    auto* shortcut_input_handler = Shell::Get()->shortcut_input_handler();
    if (shortcut_input_handler) {
      shortcut_input_handler->AddObserver(this);
    }
  }
}

ShortcutInputProvider::~ShortcutInputProvider() {
  if (Shell::HasInstance()) {
    auto* shortcut_input_handler = Shell::Get()->shortcut_input_handler();
    if (shortcut_input_handler) {
      shortcut_input_handler->SetShouldConsumeKeyEvents(
          /*should_consume_key_events=*/false);
      shortcut_input_handler->RemoveObserver(this);
    }
    Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
        /*prevent_processing_accelerators=*/false);
  }

  if (widget_) {
    widget_->RemoveObserver(this);
  }
}

void ShortcutInputProvider::BindInterface(
    mojo::PendingReceiver<common::mojom::ShortcutInputProvider> receiver) {
  CHECK(features::IsPeripheralCustomizationEnabled() ||
        ::features::IsShortcutCustomizationEnabled() ||
        ::features::IsAccessibilityFaceGazeEnabled());
  if (shortcut_input_receiver_.is_bound()) {
    shortcut_input_receiver_.reset();
  }
  shortcut_input_receiver_.Bind(std::move(receiver));
}

void ShortcutInputProvider::OnShortcutInputEventPressed(
    const mojom::KeyEvent& key_event) {
  if (observing_paused_ || prerewritten_event_.is_null()) {
    return;
  }

  for (auto& observer : shortcut_input_observers_) {
    observer->OnShortcutInputEventPressed(prerewritten_event_.Clone(),
                                          key_event.Clone());
  }
  prerewritten_event_.reset();
}

void ShortcutInputProvider::OnShortcutInputEventReleased(
    const mojom::KeyEvent& key_event) {
  if (observing_paused_ || prerewritten_event_.is_null()) {
    return;
  }

  for (auto& observer : shortcut_input_observers_) {
    observer->OnShortcutInputEventReleased(prerewritten_event_.Clone(),
                                           key_event.Clone());
  }
  prerewritten_event_.reset();
}

void ShortcutInputProvider::OnPrerewrittenShortcutInputEventPressed(
    const mojom::KeyEvent& key_event) {
  // We discard the event if it is a function key, so no "post rewrite" event
  // will be seen.
  if (key_event.vkey == ui::VKEY_FUNCTION) {
    for (auto& observer : shortcut_input_observers_) {
      observer->OnShortcutInputEventPressed(key_event.Clone(), nullptr);
    }
    return;
  }
  prerewritten_event_ = key_event.Clone();
}

void ShortcutInputProvider::OnPrerewrittenShortcutInputEventReleased(
    // We discard the event if it is a function key, so no "post rewrite" event
    // will be seen.
    const mojom::KeyEvent& key_event) {
  if (key_event.vkey == ui::VKEY_FUNCTION) {
    for (auto& observer : shortcut_input_observers_) {
      observer->OnShortcutInputEventReleased(key_event.Clone(), nullptr);
    }
    return;
  }
  prerewritten_event_ = key_event.Clone();
}

void ShortcutInputProvider::StartObservingShortcutInput(
    mojo::PendingRemote<common::mojom::ShortcutInputObserver> observer) {
  shortcut_input_observers_.Add(std::move(observer));
  AdjustShortcutBlockingIfNeeded();
}

void ShortcutInputProvider::StopObservingShortcutInput() {
  shortcut_input_observers_.Clear();
  AdjustShortcutBlockingIfNeeded();
}

void ShortcutInputProvider::OnWidgetVisibilityChanged(views::Widget* widget,
                                                      bool visible) {
  HandleObserving();
}

void ShortcutInputProvider::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  HandleObserving();
}

void ShortcutInputProvider::OnWidgetDestroying(views::Widget* widget) {
  widget_->RemoveObserver(this);
  widget_ = nullptr;
  observing_paused_ = true;
  AdjustShortcutBlockingIfNeeded();
}

void ShortcutInputProvider::HandleObserving() {
  if (!widget_) {
    return;
  }

  const bool widget_open = !widget_->IsClosed();
  const bool widget_active = widget_->IsActive();
  const bool widget_visible = widget_->IsVisible();
  observing_paused_ = !(widget_open && widget_visible && widget_active);
  AdjustShortcutBlockingIfNeeded();
}

void ShortcutInputProvider::TieProviderToWidget(views::Widget* widget) {
  if (widget_ == widget) {
    return;
  }

  CHECK(!widget_);
  CHECK(widget);
  widget_ = widget;
  widget_->AddObserver(this);
  HandleObserving();
}

void ShortcutInputProvider::AdjustShortcutBlockingIfNeeded() {
  if (!observing_paused_ && !shortcut_input_observers_.empty()) {
    Shell::Get()->shortcut_input_handler()->SetShouldConsumeKeyEvents(
        /*should_consume_key_events=*/true);
    Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
        /*prevent_processing_accelerators=*/true);
    return;
  }

  Shell::Get()->shortcut_input_handler()->SetShouldConsumeKeyEvents(
      /*should_consume_key_events=*/false);
  Shell::Get()->accelerator_controller()->SetPreventProcessingAccelerators(
      /*prevent_processing_accelerators=*/false);
}

void ShortcutInputProvider::FlushMojoForTesting() {
  shortcut_input_observers_.FlushForTesting();  // IN-TEST
}

}  // namespace ash
