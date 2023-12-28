// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COMMON_BACKEND_SHORTCUT_INPUT_PROVIDER_H_
#define ASH_WEBUI_COMMON_BACKEND_SHORTCUT_INPUT_PROVIDER_H_

#include "ash/accelerators/shortcut_input_handler.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/webui/common/mojom/shortcut_input_provider.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

// ShortcutInputProvider is used in both the Shortcut Customization SWA as well
// as the Settings SWA for the user of communicating events over mojo to
// customize shortcuts.
class ShortcutInputProvider : public common::mojom::ShortcutInputProvider,
                              public ShortcutInputHandler::Observer,
                              public views::WidgetObserver {
 public:
  ShortcutInputProvider();
  ~ShortcutInputProvider() override;

  void BindInterface(
      mojo::PendingReceiver<common::mojom::ShortcutInputProvider> receiver);

  // Ties this instance of the ShortcutInputProvider to a specific
  // `views::Widget`. Shortcut input will only ever be sent over mojo if the
  // window has focus, is active, and is still open.
  void TieProviderToWidget(views::Widget* widget);

  // mojom::ShortcutInputProvider:
  void StartObservingShortcutInput(
      mojo::PendingRemote<common::mojom::ShortcutInputObserver> observer)
      override;
  void StopObservingShortcutInput() override;

  // ShortcutInputHandler::Observer:
  void OnShortcutInputEventPressed(const mojom::KeyEvent& key_event) override;
  void OnShortcutInputEventReleased(const mojom::KeyEvent& key_event) override;
  void OnPrerewrittenShortcutInputEventPressed(
      const mojom::KeyEvent& key_event) override;
  void OnPrerewrittenShortcutInputEventReleased(
      const mojom::KeyEvent& key_event) override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  void FlushMojoForTesting();

 private:
  void HandleObserving();
  void AdjustShortcutBlockingIfNeeded();

  // Observing is only unpaused when the target window has focus, is visible,
  // and is open.
  bool observing_paused_ = true;

  // A clone of the most recent prerewritten key event.
  mojom::KeyEventPtr prerewritten_event_;

  raw_ptr<views::Widget> widget_ = nullptr;

  mojo::RemoteSet<common::mojom::ShortcutInputObserver>
      shortcut_input_observers_;
  mojo::Receiver<common::mojom::ShortcutInputProvider> shortcut_input_receiver_{
      this};
};

}  // namespace ash

#endif  // ASH_WEBUI_COMMON_BACKEND_SHORTCUT_INPUT_PROVIDER_H_
