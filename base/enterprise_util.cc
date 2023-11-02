// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/enterprise_util.h"

namespace base {

bool IsManagedOrEnterpriseDevice() {
  return IsManagedDevice() || IsEnterpriseDevice();
}

}  // namespace base
