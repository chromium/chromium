// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_host.h"

namespace chromeos {

// static
LoginDisplayHost* LoginDisplayHost::default_host_ = nullptr;

LoginDisplayHost::LoginDisplayHost() {
  DCHECK(default_host() == nullptr);
  default_host_ = this;
}

LoginDisplayHost::~LoginDisplayHost() {
  default_host_ = nullptr;
}

}  // namespace chromeos
