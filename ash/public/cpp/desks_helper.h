// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESKS_HELPER_H_
#define ASH_PUBLIC_CPP_DESKS_HELPER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Interface for an ash client (e.g. Chrome) to interact with the Virtual Desks
// feature.
class ASH_PUBLIC_EXPORT DesksHelper {
 public:
  static DesksHelper* Get();

  // Returns true if |window| exists on the currently active desk.
  virtual bool BelongsToActiveDesk(aura::Window* window) = 0;

 protected:
  DesksHelper();
  virtual ~DesksHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(DesksHelper);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESKS_HELPER_H_
