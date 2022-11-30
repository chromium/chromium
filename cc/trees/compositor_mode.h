// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_COMPOSITOR_MODE_H_
#define CC_TREES_COMPOSITOR_MODE_H_

namespace cc {

// The LayerTreeHost uses the CompositorMode to determine the current mode of
// operation, which is needed to:
// 1) Safely cast Proxy to SingleThreadProxy to allow operations only supported
// in SingleThreaded mode.
// 2) Make decisions restricted to either browser(SingleThreaded) or renderer
// compositors(Threaded).
enum class CompositorMode {
  // The main and impl components will be run on the same thread.
  SINGLE_THREADED,

  // The main and impl components be run on different threads.
  THREADED,
};

}  // namespace cc

#endif  // CC_TREES_COMPOSITOR_MODE_H_
