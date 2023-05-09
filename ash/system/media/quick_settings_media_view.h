// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_
#define ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_

#include <list>
#include <map>

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace global_media_controls {
class MediaItemUIView;
}  // namespace global_media_controls

namespace ash {

namespace {
class MediaScrollView;
}  // namespace

class PaginationController;
class PaginationModel;
class PaginationView;
class QuickSettingsMediaViewController;

// Media view displayed in the quick settings view.
class ASH_EXPORT QuickSettingsMediaView : public views::View {
 public:
  explicit QuickSettingsMediaView(QuickSettingsMediaViewController* controller);
  QuickSettingsMediaView(const QuickSettingsMediaView&) = delete;
  QuickSettingsMediaView& operator=(const QuickSettingsMediaView&) = delete;
  ~QuickSettingsMediaView() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Shows the given media item in the media view.
  void ShowItem(const std::string& id,
                std::unique_ptr<global_media_controls::MediaItemUIView> item);

  // Removes the given media item from the media view.
  void HideItem(const std::string& id);

  // Updates the media item order given the id order in the list.
  void UpdateItemOrder(std::list<std::string> ids);

 private:
  raw_ptr<QuickSettingsMediaViewController> controller_ = nullptr;

  std::unique_ptr<PaginationModel> pagination_model_;

  std::unique_ptr<PaginationController> pagination_controller_;

  raw_ptr<MediaScrollView> media_scroll_view_ = nullptr;

  raw_ptr<PaginationView> pagination_view_ = nullptr;

  std::map<const std::string, global_media_controls::MediaItemUIView*> items_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_
