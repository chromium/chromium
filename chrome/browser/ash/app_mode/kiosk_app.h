// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

// Metadata relevant to identify and display a Kiosk app.
class KioskApp {
 public:
  KioskApp(const KioskAppId& id,
           std::string_view name,
           gfx::ImageSkia icon,
           std::optional<GURL> url = std::nullopt);
  KioskApp(const KioskApp&);
  KioskApp(KioskApp&&);
  ~KioskApp();

  KioskApp& operator=(const KioskApp&);
  KioskApp& operator=(KioskApp&&);

  // The Kiosk id used to identify the app, and determine its type.
  const KioskAppId& id() const { return id_; }
  // The application name as displayed in the UI.
  std::string_view name() const { return name_; }
  // The application icon as displayed in the UI.
  gfx::ImageSkia icon() const { return icon_; }
  // The application URL as displayed in the UI. Only present in web apps.
  const std::optional<GURL>& url() const { return url_; }

 private:
  KioskAppId id_;
  std::string name_;
  gfx::ImageSkia icon_;
  std::optional<GURL> url_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_H_
