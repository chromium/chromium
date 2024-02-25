// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/test_accelerator_prefs_delegate.h"

namespace ash {

bool TestAcceleratorPrefsDelegate::IsUserEnterpriseManaged() const {
  return true;
}

}  // namespace ash
