// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_GROUP_INDICATOR_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_GROUP_INDICATOR_LAYER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/compositor/layer/layer.h"

namespace cc::slim {
class Layer;
class SolidColorLayer;
}  // namespace cc::slim

namespace android {

class LayerTitleCache;

class GroupIndicatorLayer : public Layer {
 public:
  static scoped_refptr<GroupIndicatorLayer> Create(
      LayerTitleCache* layer_title_cache);
  static void SetConstants(int reorder_background_top_margin,
                           int reorder_background_bottom_margin,
                           int reorder_background_padding_start,
                           int reorder_background_padding_end,
                           int reorder_background_corner_radius);

  GroupIndicatorLayer(const GroupIndicatorLayer&) = delete;
  GroupIndicatorLayer& operator=(const GroupIndicatorLayer&) = delete;

  void SetProperties(int id,
                     int tint,
                     int reorder_background_tint,
                     int bubble_tint,
                     bool incognito,
                     bool foreground,
                     bool show_bubble,
                     bool show_reorder_background,
                     float x,
                     float y,
                     float width,
                     float height,
                     float title_start_padding,
                     float title_end_padding,
                     float corner_radius,
                     float bottom_indicator_width,
                     float bottom_indicator_height,
                     float bubble_padding,
                     float bubble_size,
                     float tab_strip_height);
  bool foreground();
  scoped_refptr<cc::slim::Layer> layer() override;

 protected:
  explicit GroupIndicatorLayer(LayerTitleCache* layer_title_cache);
  ~GroupIndicatorLayer() override;

 private:
  static inline int reorder_background_top_margin_;
  static inline int reorder_background_bottom_margin_;
  static inline int reorder_background_padding_start_;
  static inline int reorder_background_padding_end_;
  static inline int reorder_background_corner_radius_;

  raw_ptr<LayerTitleCache> layer_title_cache_;

  scoped_refptr<cc::slim::Layer> layer_;
  scoped_refptr<cc::slim::SolidColorLayer> reorder_background_;
  scoped_refptr<cc::slim::SolidColorLayer> group_indicator_;
  scoped_refptr<cc::slim::SolidColorLayer> bottom_outline_;
  scoped_refptr<cc::slim::SolidColorLayer> notification_bubble_;
  scoped_refptr<cc::slim::Layer> title_layer_;

  bool foreground_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_GROUP_INDICATOR_LAYER_H_
