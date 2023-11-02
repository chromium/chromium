// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ARC_RESIZE_LOCK_TYPE_H_
#define ASH_PUBLIC_CPP_ARC_RESIZE_LOCK_TYPE_H_

namespace ash {

// Represents how strictly resize operations are limited for a window.
// This class must strictly corresponds to the resize_lock_type enum in the
// wayland remote surface protocol.
enum class ArcResizeLockType {
  // ResizeLock is disabled and the window follows normal resizability.
  NONE = 0,

  // Resizing is enabled and resize lock type is togglable.
  RESIZE_ENABLED_TOGGLABLE = 1,

  // Resizing is disabled and resize lock type is togglable.
  RESIZE_DISABLED_TOGGLABLE = 2,

  // Resizing is disabled and resize lock type is not togglable.
  RESIZE_DISABLED_NONTOGGLABLE = 3,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ARC_RESIZE_LOCK_TYPE_H_
