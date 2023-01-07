// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_authorizationref.h"

namespace base::mac {

void ScopedAuthorizationRef::FreeInternal() {
  AuthorizationFree(authorization_, kAuthorizationFlagDestroyRights);
}

}  // namespace base::mac
