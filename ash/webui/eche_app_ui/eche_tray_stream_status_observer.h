// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_ECHE_TRAY_STREAM_STATUS_OBSERVER_H_
#define ASH_WEBUI_ECHE_APP_UI_ECHE_TRAY_STREAM_STATUS_OBSERVER_H_

#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/eche_stream_status_change_handler.h"
#include "ash/webui/eche_app_ui/feature_status_provider.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom-shared.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace ash {
namespace eche_app {

// It is called from chrome/browser/ash/eche_app/eche_app_manager_factory.cc.
// Move all logic about Eche tray to here because we don't want to call
// `GetEcheTray` everywhere.
void LaunchBubble(const GURL& url,
                  const gfx::Image& icon,
                  const std::u16string& visible_name,
                  const std::u16string& phone_name,
                  eche_app::mojom::ConnectionStatus last_connection_status,
                  eche_app::mojom::AppStreamLaunchEntryPoint entry_point,
                  EcheTray::GracefulCloseCallback graceful_close_callback,
                  EcheTray::GracefulGoBackCallback graceful_go_back_callback,
                  EcheTray::BubbleShownCallback bubble_shown_callback);

// The observer that observes the stream status change and notifies `EcheTray`
// show/hide/close the bubble when Eche starts/stops streaming.
// TODO(b/226687249): Implement this observer in EcheTray directly if we fix the
// package dependency error between //eche_app_ui and //ash.
class EcheTrayStreamStatusObserver
    : public EcheStreamStatusChangeHandler::Observer,
      public FeatureStatusProvider::Observer {
 public:
  EcheTrayStreamStatusObserver(
      EcheStreamStatusChangeHandler* stream_status_change_handler,
      FeatureStatusProvider* feature_status_provider);
  ~EcheTrayStreamStatusObserver() override;

  EcheTrayStreamStatusObserver(const EcheTrayStreamStatusObserver&) = delete;
  EcheTrayStreamStatusObserver& operator=(const EcheTrayStreamStatusObserver&) =
      delete;

  // EcheStreamStatusChangeHandler::Observer:
  void OnStartStreaming() override;
  void OnStreamStatusChanged(mojom::StreamStatus status) override;

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override;

 private:
  raw_ptr<FeatureStatusProvider> feature_status_provider_;

  base::ScopedObservation<EcheStreamStatusChangeHandler,
                          EcheStreamStatusChangeHandler::Observer>
      observed_session_{this};
};

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_ECHE_TRAY_STREAM_STATUS_OBSERVER_H_
