// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TAP_VISUALIZER_TAP_VISUALIZER_APP_H_
#define ASH_COMPONENTS_TAP_VISUALIZER_TAP_VISUALIZER_APP_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "services/service_manager/public/cpp/service.h"
#include "ui/display/display_observer.h"
#include "ui/events/event_observer.h"

namespace views {
class AuraInit;
}  // namespace views

namespace tap_visualizer {

class TapRenderer;

// Application that paints touch tap points as circles. Creates a fullscreen
// transparent widget on each display to draw the taps.
class TapVisualizerApp : public service_manager::Service,
                         public ui::EventObserver,
                         public display::DisplayObserver {
 public:
  TapVisualizerApp();
  ~TapVisualizerApp() override;

 private:
  friend class TapVisualizerAppTestApi;

  // Starts showing touches on all displays.
  void Start();

  // service_manager::Service:
  void OnStart() override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;

  // Creates the touch HUD widget for a display.
  void CreateWidgetForDisplay(int64_t display_id);

  // Maps display::Display::id() to the renderer for that display.
  std::map<int64_t, std::unique_ptr<TapRenderer>> display_id_to_renderer_;

  std::unique_ptr<views::AuraInit> aura_init_;

  DISALLOW_COPY_AND_ASSIGN(TapVisualizerApp);
};

}  // namespace tap_visualizer

#endif  // ASH_COMPONENTS_TAP_VISUALIZER_TAP_VISUALIZER_APP_H_
