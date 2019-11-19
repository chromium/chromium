// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_TYPES_H_
#define ASH_WM_WORKSPACE_WORKSPACE_TYPES_H_

namespace ash {

// Enumeration of the possible window states.
enum class WorkspaceWindowState {
  // There's a full screen window.
  kFullscreen,

  // There's a maximized window.
  kMaximized,

  // None of the windows are fullscreen, maximized or touch the shelf.
  kDefault,
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_TYPES_H_
