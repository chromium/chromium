// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_ICON_TITLE_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_ICON_TITLE_H_

#include <jni.h>

#include <memory>

#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/decoration_title.h"  // Base class
#include "ui/android/resources/resource_manager.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace android {

class DecorationIconTitle : public DecorationTitle {
 public:
  DecorationIconTitle(ui::ResourceManager* resource_manager,
                      int title_resource_id,
                      int icon_resource_id,
                      int fade_width,
                      int icon_start_padding,
                      int icon_end_padding,
                      bool is_incognito,
                      bool is_rtl);

  ~DecorationIconTitle() override;

  void Update(int title_resource_id,
              int icon_resource_id,
              int fade_width,
              int icon_start_padding,
              int icon_end_padding,
              bool is_incognito,
              bool is_rtl);
  void SetIconResourceId(int icon_resource_id);
  void SetUIResourceIds() override;
  void setBounds(const gfx::Size& bounds) override;
  void setOpacity(float opacity) override;
  const gfx::Size& size() { return size_; }

 protected:
  void handleIconResource(ui::AndroidResourceType resource_type);
  gfx::Size calculateSize(int icon_with) override;

  scoped_refptr<cc::slim::UIResourceLayer> layer_icon_;

  gfx::Size icon_size_;
  gfx::Size size_;

  int icon_resource_id_;
  int icon_start_padding_;
  int icon_end_padding_;
  std::unique_ptr<gfx::Transform> transform_;
  gfx::PointF icon_position_;
  bool icon_needs_refresh_ = true;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_ICON_TITLE_H_
