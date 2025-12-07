// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TAB_TITLE_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TAB_TITLE_H_

#include <jni.h>

#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/decoration_icon_title.h"  // Base class
#include "ui/gfx/geometry/size.h"

namespace cc::slim {
class SolidColorLayer;
}  // namespace cc::slim

namespace android {

class DecorationTabTitle : public DecorationIconTitle {
 public:
  DecorationTabTitle(ui::ResourceManager* resource_manager,
                     int title_resource_id,
                     int icon_resource_id,
                     int spinner_resource_id,
                     int spinner_resource_id_incognito,
                     int fade_width,
                     int icon_start_padding,
                     int icon_end_padding,
                     bool is_incognito,
                     bool is_rtl,
                     bool show_bubble,
                     int bubble_inner_dimension,
                     int bubble_outer_dimension,
                     int bubble_offset,
                     int bubble_inner_tint,
                     int bubble_outer_tint);

  ~DecorationTabTitle() override;

  void SetUIResourceIds() override;
  void SetIsLoading(bool is_loading);
  void SetSpinnerRotation(float rotation);
  void SetShouldHideTitleText(bool should_hide_title_text);
  void SetShouldHideIcon(bool should_hide_icon);
  void setBounds(const gfx::Size& bounds) override;
  void Update(int title_resource_id,
              int icon_resource_id,
              int fade_width,
              int icon_start_padding,
              int icon_end_padding,
              bool is_incognito,
              bool is_rtl,
              bool show_bubble);
  void SetShowBubble(bool show_bubble);
  const gfx::Size& size() { return size_; }

 private:
  int spinner_resource_id_;
  int spinner_incognito_resource_id_;
  bool show_bubble_;
  int bubble_inner_dimension_;
  int bubble_outer_dimension_;
  int bubble_offset_;
  int bubble_inner_tint_;
  int bubble_outer_tint_;
  scoped_refptr<cc::slim::SolidColorLayer> tab_bubble_outer_circle_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> tab_bubble_inner_circle_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> CreateTabBubbleCircle(int size,
                                                                 int tint);
  void CreateTabBubble();
  void CreateAndShowTabBubble(gfx::PointF position);
  void HideTabBubble();

  gfx::Size size_;
  float spinner_rotation_ = 0;
  bool is_loading_ = false;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TAB_TITLE_H_
