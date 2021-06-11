// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ARC_RESIZE_LOCK_TYPE_H_
#define ASH_PUBLIC_CPP_ARC_RESIZE_LOCK_TYPE_H_

namespace ash {

// Represents how strictly resize operations are limited for a window.
enum class ArcResizeLockType {
  // The resizability is not restricted by the resize lock feature.
  RESIZABLE = 0,

  // The app is only allowed to be resized via limited operations such as via
  // the compatibility mode dialog.
  RESIZE_LIMITED = 1,

  // No explicit resize operation is allowed. (The window can still be resized,
  // e.g. when the display scale factor changes.)
  FULLY_LOCKED = 2,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ARC_RESIZE_LOCK_TYPE_H_
