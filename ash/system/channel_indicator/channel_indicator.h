// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_
#define ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_

#include <string>

#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/session_manager/session_manager_types.h"
#include "components/version_info/channel.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class BoxLayout;
}  // namespace views

namespace ash {

class Shell;

// A view that resides in the system tray, to make it obvious to the user when a
// device is running on a release track other than "stable."
class ASH_EXPORT ChannelIndicatorView : public TrayItemView,
                                        public SessionObserver,
                                        public ShellObserver {
  METADATA_HEADER(ChannelIndicatorView, TrayItemView)

 public:
  ChannelIndicatorView(Shelf* shelf, version_info::Channel channel);
  ChannelIndicatorView(const ChannelIndicatorView&) = delete;
  ChannelIndicatorView& operator=(const ChannelIndicatorView&) = delete;

  ~ChannelIndicatorView() override;

  // views::View:
  views::View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  std::u16string GetTooltipText(const gfx::Point& p) const override;
  void OnThemeChanged() override;

  // TrayItemView:
  void HandleLocaleChange() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // Introspection methods for testing.
  bool IsLabelVisibleForTesting();
  bool IsImageViewVisibleForTesting();

  // Returns the accessibility name.
  std::u16string GetAccessibleNameString() const;

 private:
  void Update();
  void SetImageOrText();
  void OnAccessibleNameChanged(const std::u16string& new_name) override;
  void SetTooltip();

  // The localized string displayed when this view is hovered-over.
  std::u16string tooltip_;

  // The release track on which this devices resides.
  const version_info::Channel channel_;

  // The `BoxLayout` with which the `TrayItemView`-provided `FillLayout` is
  // replaced, owned by `views::View`. `FillLayout` wants to size child views to
  // fit the parent's bounds, but children of `ChannelIndicatorView` need to
  // have specific sizes and insets regardless of the parent's bounds.
  raw_ptr<views::BoxLayout> box_layout_;

  ScopedSessionObserver session_observer_;

  base::ScopedObservation<Shell, ShellObserver> shell_observer_{this};

  base::WeakPtrFactory<ChannelIndicatorView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHANNEL_INDICATOR_CHANNEL_INDICATOR_H_
