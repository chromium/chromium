// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace ash {

class RefreshBannerView;

// The widget that contains the Mahi panel.
// TODO(b/319329379): Use this class in `CreatePanelWidget()` when resizing and
// closing capability is added.
class ASH_EXPORT MahiPanelWidget : public views::Widget, views::ViewObserver {
 public:
  explicit MahiPanelWidget(InitParams params);

  MahiPanelWidget(const MahiPanelWidget&) = delete;
  MahiPanelWidget& operator=(const MahiPanelWidget&) = delete;

  ~MahiPanelWidget() override;

  // Creates the Mahi panel widget within the display with `display_id`.
  static views::UniqueWidgetPtr CreatePanelWidget(int64_t display_id);

  static const char* GetName();

  // Notifies observers through the UI controller that availability for a
  // content refresh has changed.
  void NotifyRefreshAvailabilityChanged(bool available);

  // Sends `question` to the backend. `current_panel_content` determines if the
  // `question` is regarding the current content displayed on the panel.
  void SendQuestion(const std::u16string& question, bool current_panel_content);

 private:
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  MahiUiController ui_controller_;

  // Owned by views hierarchy.
  raw_ptr<RefreshBannerView> refresh_view_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver>
      refresh_view_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_
