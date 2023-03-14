// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_STREAM_ORIENTATION_OBSERVER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_STREAM_ORIENTATION_OBSERVER_H_

#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::eche_app {

class EcheStreamOrientationObserver : public mojom::StreamOrientationObserver {
 public:
  EcheStreamOrientationObserver();
  ~EcheStreamOrientationObserver() override;

  EcheStreamOrientationObserver(const EcheStreamOrientationObserver&) = delete;
  EcheStreamOrientationObserver& operator=(
      const EcheStreamOrientationObserver&) = delete;

  // mojom::StreamOrientationObserver
  void OnStreamOrientationChanged(bool is_landscape) override;

  void Bind(mojo::PendingReceiver<mojom::StreamOrientationObserver> receiver);

 private:
  mojo::Receiver<mojom::StreamOrientationObserver> stream_orientation_receiver_{
      this};
};

}  // namespace ash::eche_app

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_STREAM_ORIENTATION_OBSERVER_H_
