// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app.h"

#include <string>

#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

KioskApp::KioskApp(const KioskAppId& id,
                   const std::string& name,
                   gfx::ImageSkia icon)
    : id_(id), name_(name), icon_(icon) {}

KioskApp::KioskApp(const KioskApp&) = default;
KioskApp::KioskApp(KioskApp&&) = default;

KioskApp::~KioskApp() = default;

KioskApp& KioskApp::operator=(const KioskApp&) = default;
KioskApp& KioskApp::operator=(KioskApp&&) = default;

}  // namespace ash
