// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_authorizationref.h"

namespace base {
namespace mac {

void ScopedAuthorizationRef::FreeInternal() {
  AuthorizationFree(authorization_, kAuthorizationFlagDestroyRights);
}

}  // namespace mac
}  // namespace base
