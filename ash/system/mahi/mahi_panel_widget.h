// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_
#define ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_

#include "ash/ash_export.h"
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

  // Shows/hides the refresh UI in the panel.
  void SetRefreshViewVisible(bool visible);

 private:
  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // Owned by views hierarchy.
  raw_ptr<RefreshBannerView> refresh_view_ = nullptr;

  base::ScopedObservation<views::View, views::ViewObserver>
      refresh_view_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_PANEL_WIDGET_H_
