// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_
#define ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_

#include <map>

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace global_media_controls {
class MediaItemUIView;
}  // namespace global_media_controls

namespace ash {

class QuickSettingsMediaViewController;

// Media view displayed in the quick settings view.
class ASH_EXPORT QuickSettingsMediaView : public views::View {
 public:
  explicit QuickSettingsMediaView(QuickSettingsMediaViewController* controller);
  QuickSettingsMediaView(const QuickSettingsMediaView&) = delete;
  QuickSettingsMediaView& operator=(const QuickSettingsMediaView&) = delete;
  ~QuickSettingsMediaView() override;

  // Shows the given media item in the media view.
  void ShowItem(const std::string& id,
                std::unique_ptr<global_media_controls::MediaItemUIView> item);

  // Removes the given media item from the media view.
  void HideItem(const std::string& id);

 private:
  raw_ptr<QuickSettingsMediaViewController> controller_ = nullptr;

  std::map<const std::string, global_media_controls::MediaItemUIView*> items_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_QUICK_SETTINGS_MEDIA_VIEW_H_
