// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TAB_TITLE_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TAB_TITLE_H_

#include <jni.h>

#include "cc/slim/ui_resource_layer.h"
#include "chrome/browser/android/compositor/decoration_icon_title.h"  // Base class
#include "ui/gfx/geometry/size.h"

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
                     bool is_rtl);

  ~DecorationTabTitle() override;

  void SetUIResourceIds() override;
  void SetIsLoading(bool is_loading);
  void SetSpinnerRotation(float rotation);
  const gfx::Size& size() { return size_; }

 private:
  int spinner_resource_id_;
  int spinner_incognito_resource_id_;

  gfx::Size size_;
  float spinner_rotation_ = 0;
  bool is_loading_ = false;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TAB_TITLE_H_
