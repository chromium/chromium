// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TITLE_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TITLE_H_

#include <jni.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
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
                  int fade_width,
                  bool is_incognito,
                  bool is_rtl);

  DecorationTitle(const DecorationTitle&) = delete;
  DecorationTitle& operator=(const DecorationTitle&) = delete;

  virtual ~DecorationTitle();

  virtual void SetResourceManager(ui::ResourceManager* resource_manager);

  void Update(int title_resource_id,
              int fade_width,
              bool is_incognito,
              bool is_rtl);
  virtual void SetUIResourceIds();
  virtual void setBounds(const gfx::Size& bounds);
  virtual void setOpacity(float opacity);

  scoped_refptr<cc::slim::Layer> layer();
  const gfx::Size& size() { return size_; }

 protected:
  void setBounds(const gfx::Size& bounds, int start_space);
  virtual gfx::Size calculateSize(int favicon_width);

  scoped_refptr<cc::slim::Layer> layer_;
  scoped_refptr<cc::slim::UIResourceLayer> layer_opaque_;
  scoped_refptr<cc::slim::UIResourceLayer> layer_fade_;

  int title_resource_id_;
  int fade_width_;

  gfx::Size title_size_;
  gfx::Size size_;
  bool is_incognito_;
  bool is_rtl_;

  raw_ptr<ui::ResourceManager> resource_manager_;

 private:
  bool needs_refresh_ = true;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_DECORATION_TITLE_H_
