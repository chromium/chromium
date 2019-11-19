// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/gaia_screen.h"

#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"

namespace chromeos {

void GaiaScreen::MaybePreloadAuthExtension() {
  view_->MaybePreloadAuthExtension();
}

}  // namespace chromeos
