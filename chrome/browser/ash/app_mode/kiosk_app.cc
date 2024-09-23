// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

KioskApp::KioskApp(const KioskAppId& id,
                   std::string_view name,
                   gfx::ImageSkia icon,
                   std::optional<GURL> url)
    : id_(id), name_(name), icon_(icon), url_(std::move(url)) {
  bool should_have_url = id_.type == KioskAppType::kWebApp;
  CHECK_EQ(should_have_url, url.has_value());
}

KioskApp::KioskApp(const KioskApp&) = default;
KioskApp::KioskApp(KioskApp&&) = default;

KioskApp::~KioskApp() = default;

KioskApp& KioskApp::operator=(const KioskApp&) = default;
KioskApp& KioskApp::operator=(KioskApp&&) = default;

}  // namespace ash
