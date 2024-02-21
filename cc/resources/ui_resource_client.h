// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_UI_RESOURCE_CLIENT_H_
#define CC_RESOURCES_UI_RESOURCE_CLIENT_H_

#include "base/functional/callback.h"
#include "cc/cc_export.h"

namespace cc {

class UIResourceBitmap;

typedef int UIResourceId;

class CC_EXPORT UIResourceClient {
 public:
  static constexpr UIResourceId kUninitializedUIResourceId = -1;

  // GetBitmap() will be called once soon after resource creation and then will
  // be called afterwards whenever the GL context is lost, on the same thread
  // that LayerTreeHost::CreateUIResource was called on.  It is only safe to
  // delete a UIResourceClient object after DeleteUIResource has been called for
  // all IDs associated with it.  A valid bitmap always must be returned but it
  // doesn't need to be the same size or format as the original.
  // The bitmap's dimensions must *not* exceed the maximum texture size
  // supported by the GPU. For resources that are not bigger than the viewport
  // this should not be a problem, but for much larger resources, the caller is
  // responsible for ensuring this.
  virtual UIResourceBitmap GetBitmap(UIResourceId uid,
                                     bool resource_lost) = 0;
  virtual ~UIResourceClient() {}
};

}  // namespace cc

#endif  // CC_RESOURCES_UI_RESOURCE_CLIENT_H_
