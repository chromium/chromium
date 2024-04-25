// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_CONTAINER_H_
#define ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_CONTAINER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class UnifiedSystemTrayController;

// Container view of QuickSettingsMediaView which manages the visibility of the
// entire quick settings media view.
class QuickSettingsMediaViewContainer : public views::View {
  METADATA_HEADER(QuickSettingsMediaViewContainer, views::View)

 public:
  explicit QuickSettingsMediaViewContainer(
      UnifiedSystemTrayController* controller);
  QuickSettingsMediaViewContainer(const QuickSettingsMediaViewContainer&) =
      delete;
  QuickSettingsMediaViewContainer& operator=(
      const QuickSettingsMediaViewContainer&) = delete;
  ~QuickSettingsMediaViewContainer() override = default;

  // Sets whether the quick settings view should show the media view.
  void SetShowMediaView(bool show_media_view);

  // Maybe show the media view when user navigates back to the quick settings
  // view from the detailed media view.
  void MaybeShowMediaView();

  // Returns the media view height based on its visibility.
  int GetExpandedHeight() const;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  const raw_ptr<UnifiedSystemTrayController> controller_;

  bool show_media_view_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_CONTAINER_H_
