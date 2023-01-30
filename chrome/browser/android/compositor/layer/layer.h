// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_LAYER_H_

#include "base/memory/ref_counted.h"
#include "cc/slim/layer.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// A simple wrapper for cc::Layer. You can override this to contain
// layers and add functionalities to it.
class Layer : public base::RefCounted<Layer> {
 public:
  Layer(const Layer&) = delete;
  Layer& operator=(const Layer&) = delete;

  virtual scoped_refptr<cc::slim::Layer> layer() = 0;

 protected:
  Layer() {}
  virtual ~Layer() {}

 private:
  friend class base::RefCounted<Layer>;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_LAYER_H_
