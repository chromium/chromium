// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_DELEGATE_H_
#define ASH_GLANCEABLES_GLANCEABLES_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

// Delegate interface for GlanceablesController. Implemented by an ash client
// (e.g. Chrome). Allows access to functionality at the //chrome/browser layer.
// Owned by the GlanceablesController.
class ASH_EXPORT GlanceablesDelegate {
 public:
  virtual ~GlanceablesDelegate() = default;

  // Triggers session restore.
  virtual void RestoreSession() = 0;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_DELEGATE_H_
