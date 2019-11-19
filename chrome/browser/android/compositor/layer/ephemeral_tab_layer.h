// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_EPHEMERAL_TAB_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_EPHEMERAL_TAB_LAYER_H_

#include "chrome/browser/android/compositor/layer/overlay_panel_layer.h"

#include "base/callback.h"
#include "components/favicon/core/favicon_driver_observer.h"

namespace base {
class CancelableTaskTracker;
}

namespace cc {
class Layer;
}

namespace content {
class WebContents;
}

namespace favicon {
class FaviconDriver;
}

namespace ui {
class ResourceManager;
}

namespace android {
class EphemeralTabLayer : public OverlayPanelLayer,
                          public favicon::FaviconDriverObserver {
 public:
  static constexpr float kFaviconWidthDp =
      OverlayPanelLayer::kDefaultIconWidthDp;

  // Scale factor used to make the security icon size smaller to fit in the
  // header.
  static constexpr float kSecurityIconScale = 0.8f;

  // Left margin that positions the icon in front of the caption.
  static constexpr float kSecurityIconMarginStartDp = 8.f;

  static scoped_refptr<EphemeralTabLayer> Create(
      ui::ResourceManager* resource_manager,
      base::RepeatingCallback<void()>&& favicon_callback);
  void SetProperties(content::WebContents* web_contents,
                     int title_view_resource_id,
                     int caption_view_resource_id,
                     int security_icon_resource_id,
                     jfloat security_icon_opacity,
                     jfloat caption_animation_percentage,
                     jfloat text_layer_min_height,
                     jfloat title_caption_spacing,
                     jboolean caption_visible,
                     int progress_bar_background_resource_id,
                     int progress_bar_resource_id,
                     float dp_to_px,
                     const scoped_refptr<cc::Layer>& content_layer,
                     float panel_x,
                     float panel_y,
                     float panel_width,
                     float panel_height,
                     int bar_background_color,
                     float bar_margin_side,
                     float bar_margin_top,
                     float bar_height,
                     bool bar_border_visible,
                     float bar_border_height,
                     int icon_color,
                     int drag_handlebar_color,
                     jfloat favicon_opacity,
                     bool progress_bar_visible,
                     float progress_bar_height,
                     float progress_bar_opacity,
                     float progress_bar_completion,
                     int separator_line_color,
                     bool is_new_layout);
  void SetupTextLayer(float bar_top,
                      float bar_height,
                      float text_layer_min_height,
                      int caption_view_resource_id,
                      int security_icon_resource_id,
                      float security_icon_opacity,
                      float animation_percentage,
                      bool caption_visible,
                      int context_resource_id,
                      float title_caption_spacing);

  void OnHide();

  // favicon::FaviconDriverObserver
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 protected:
  EphemeralTabLayer(ui::ResourceManager* resource_manager,
                    base::RepeatingCallback<void()>&& favicon_callback);
  ~EphemeralTabLayer() override;

 private:
  content::WebContents* web_contents_ = nullptr;
  float dp_to_px_;
  float panel_width_;
  float bar_height_;
  float bar_margin_side_;
  std::string favicon_url_host_;
  base::RepeatingCallback<void()> favicon_callback_;
  scoped_refptr<cc::UIResourceLayer> title_;
  scoped_refptr<cc::UIResourceLayer> caption_;
  scoped_refptr<cc::UIResourceLayer> favicon_layer_;
  scoped_refptr<cc::UIResourceLayer> security_icon_layer_;
  scoped_refptr<cc::UIResourceLayer> text_layer_;
  std::unique_ptr<base::CancelableTaskTracker> cancelable_task_tracker_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_EPHEMERAL_TAB_LAYER_H_
