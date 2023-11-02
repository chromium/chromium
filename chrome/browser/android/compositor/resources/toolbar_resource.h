// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_RESOURCES_TOOLBAR_RESOURCE_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_RESOURCES_TOOLBAR_RESOURCE_H_

#include "ui/android/resources/resource.h"

namespace android {

class ToolbarResource final : public ui::Resource {
 public:
  static ToolbarResource* From(ui::Resource* resource);

  ToolbarResource(gfx::Rect toolbar_rect,
                  gfx::Rect location_bar_content_rect,
                  int shadow_height);
  ~ToolbarResource() override;

  std::unique_ptr<ui::Resource> CreateForCopy() override;

  gfx::Rect toolbar_rect() const { return toolbar_rect_; }
  gfx::Rect location_bar_content_rect() const {
    return location_bar_content_rect_;
  }
  int shadow_height() const { return shadow_height_; }

 private:
  // All rects are in the Toolbar container's space.
  gfx::Rect toolbar_rect_;
  gfx::Rect location_bar_content_rect_;
  int shadow_height_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_RESOURCES_TOOLBAR_RESOURCE_H_
