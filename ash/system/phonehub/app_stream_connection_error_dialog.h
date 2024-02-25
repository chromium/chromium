// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_APP_STREAM_CONNECTION_ERROR_DIALOG_H_
#define ASH_SYSTEM_PHONEHUB_APP_STREAM_CONNECTION_ERROR_DIALOG_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/events/event.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Rect;
}

using StartTetheringCallback = base::OnceCallback<void(const ui::Event&)>;

namespace ash {

// A view to anchor error dialog to Phone Hub bubble.
class ASH_EXPORT AppStreamConnectionErrorDialog : public views::WidgetObserver,
                                                  public views::ViewObserver {
 public:
  // TODO(b/273823160): Make the callback name more generic and take in strings
  // as parameters to make the dialog available in other locations.
  AppStreamConnectionErrorDialog(
      views::View* host_view,
      base::OnceClosure on_close_callback,
      StartTetheringCallback start_tethering_callback,
      bool is_different_network,
      bool is_phone_on_cellular);
  AppStreamConnectionErrorDialog(const AppStreamConnectionErrorDialog& other) =
      delete;
  AppStreamConnectionErrorDialog& operator=(
      const AppStreamConnectionErrorDialog& other) = delete;
  ~AppStreamConnectionErrorDialog() override;

  // Repositions the dialog widget bounds relative to the current host view
  // bounds.
  void UpdateBounds();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  views::Widget* widget() { return widget_; }

 private:
  raw_ptr<views::Widget> widget_ = nullptr;

  const raw_ptr<views::View> host_view_;

  base::OnceClosure on_close_callback_;

  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};
  base::ScopedMultiSourceObservation<views::View, views::ViewObserver>
      view_observations_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_APP_STREAM_CONNECTION_ERROR_DIALOG_H_
