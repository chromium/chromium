// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service.h"

#include <ostream>

namespace apps {

std::ostream& operator<<(std::ostream& out, AppInstallSurface surface) {
  switch (surface) {
    case AppInstallSurface::kAppInstallNavigationThrottle:
      return out << "AppInstallNavigationThrottle";
  }
}

AppInstallService::~AppInstallService() = default;

}  // namespace apps
