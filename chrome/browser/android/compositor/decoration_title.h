// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TITLE_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TITLE_H_

#include <jni.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "cc/resources/ui_resource_client.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace cc::slim {
class Layer;
class UIResourceLayer;
}  // namespace cc::slim

namespace ui {
class ResourceManager;
}

namespace android {

class DecorationTitle {
 public:
  DecorationTitle(ui::ResourceManager* resource_manager,
                  int title_resource_id,
                  int favicon_resource_id,
                  int spinner_resource_id,
                  int spinner_resource_id_incognito,
                  int fade_width,
                  int favicon_start_padding,
                  int favicon_end_padding,
                  bool is_incognito,
                  bool is_rtl);

  DecorationTitle(const DecorationTitle&) = delete;
  DecorationTitle& operator=(const DecorationTitle&) = delete;

  virtual ~DecorationTitle();

  void SetResourceManager(ui::ResourceManager* resource_manager);

  void Update(int title_resource_id,
              int favicon_resource_id,
              int fade_width,
              int favicon_start_padding,
              int favicon_end_padding,
              bool is_incognito,
              bool is_rtl);
  void SetFaviconResourceId(int favicon_resource_id);
  void SetUIResourceIds();
  void SetIsLoading(bool is_loading);
  void SetSpinnerRotation(float rotation);
  void setBounds(const gfx::Size& bounds);
  void setOpacity(float opacity);

  scoped_refptr<cc::slim::Layer> layer();
  const gfx::Size& size() { return size_; }

 private:
  scoped_refptr<cc::slim::Layer> layer_;
  scoped_refptr<cc::slim::UIResourceLayer> layer_opaque_;
  scoped_refptr<cc::slim::UIResourceLayer> layer_fade_;
  scoped_refptr<cc::slim::UIResourceLayer> layer_favicon_;

  int title_resource_id_;
  int favicon_resource_id_;
  int spinner_resource_id_;
  int spinner_incognito_resource_id_;

  gfx::Size title_size_;
  gfx::Size favicon_size_;
  gfx::Size size_;
  int fade_width_;
  float spinner_rotation_;
  int favicon_start_padding_;
  int favicon_end_padding_;
  bool is_incognito_;
  bool is_rtl_;
  bool is_loading_;
  std::unique_ptr<gfx::Transform> transform_;

  raw_ptr<ui::ResourceManager> resource_manager_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TITLE_H_
