// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_
#define ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_

#include <list>
#include <map>

#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace global_media_controls {
class MediaItemUIView;
}  // namespace global_media_controls

namespace ash {

namespace {
class MediaScrollView;
}  // namespace

class PaginationController;
class PaginationView;
class QuickSettingsMediaViewController;

// Media view displayed in the quick settings view.
class ASH_EXPORT QuickSettingsMediaView : public views::View {
  METADATA_HEADER(QuickSettingsMediaView, views::View)

 public:
  explicit QuickSettingsMediaView(QuickSettingsMediaViewController* controller);
  QuickSettingsMediaView(const QuickSettingsMediaView&) = delete;
  QuickSettingsMediaView& operator=(const QuickSettingsMediaView&) = delete;
  ~QuickSettingsMediaView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Shows the given media item in the media view.
  void ShowItem(const std::string& id,
                std::unique_ptr<global_media_controls::MediaItemUIView> item);

  // Removes the given media item from the media view.
  void HideItem(const std::string& id);

  // Updates the media item order given the id order in the list.
  void UpdateItemOrder(std::list<std::string> ids);

  // Returns the current desired height of the media view. If there are multiple
  // media items, the height needs to be larger to display the pagination view.
  int GetMediaViewHeight() const;

  // Helper functions for testing.
  PaginationModel* pagination_model_for_testing() { return &pagination_model_; }
  std::map<const std::string,
           raw_ptr<global_media_controls::MediaItemUIView, CtnExperimental>>
  items_for_testing() {
    return items_;
  }

 private:
  raw_ptr<QuickSettingsMediaViewController> controller_ = nullptr;

  PaginationModel pagination_model_{this};

  std::unique_ptr<PaginationController> pagination_controller_;

  raw_ptr<MediaScrollView> media_scroll_view_ = nullptr;

  raw_ptr<PaginationView> pagination_view_ = nullptr;

  std::map<const std::string,
           raw_ptr<global_media_controls::MediaItemUIView, CtnExperimental>>
      items_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_
