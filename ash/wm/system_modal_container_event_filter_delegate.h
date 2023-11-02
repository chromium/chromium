// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_DELEGATE_H_
#define ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_DELEGATE_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

class ASH_EXPORT SystemModalContainerEventFilterDelegate {
 public:
  // Returns true if |window| can receive the specified event.
  virtual bool CanWindowReceiveEvents(aura::Window* window) = 0;
};

}  // namespace ash

#endif  // ASH_WM_SYSTEM_MODAL_CONTAINER_EVENT_FILTER_DELEGATE_H_
