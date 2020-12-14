// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DESKS_HELPER_H_
#define ASH_PUBLIC_CPP_DESKS_HELPER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/strings/string16.h"

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

  // Returns the active desk's index.
  virtual int GetActiveDeskIndex() const = 0;

  // Returns the names of the desk at |index|. If |index| is out-of-bounds,
  // return empty string.
  virtual base::string16 GetDeskName(int index) const = 0;

  // Returns the number of desks.
  virtual int GetNumberOfDesks() const = 0;

  // Sends |window| to desk at |desk_index|. Does nothing if the desk at
  // |desk_index| is the active desk. |desk_index| must be valid.
  virtual void SendToDeskAtIndex(aura::Window* window, int desk_index) = 0;

 protected:
  DesksHelper();
  virtual ~DesksHelper();

 private:
  DISALLOW_COPY_AND_ASSIGN(DesksHelper);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DESKS_HELPER_H_
