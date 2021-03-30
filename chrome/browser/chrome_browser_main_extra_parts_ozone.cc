// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_ozone.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/lifetime/application_lifetime.h"

#if defined(USE_X11)
#include "ui/gfx/x/connection.h"  // nogncheck
#endif

#if defined(USE_OZONE)
#include "ui/base/ui_base_features.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

ChromeBrowserMainExtraPartsOzone::ChromeBrowserMainExtraPartsOzone() = default;

ChromeBrowserMainExtraPartsOzone::~ChromeBrowserMainExtraPartsOzone() = default;

void ChromeBrowserMainExtraPartsOzone::PreEarlyInitialization() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    ui::OzonePlatform::PreEarlyInitialization();
    return;
  }
#endif
}

void ChromeBrowserMainExtraPartsOzone::PostMainMessageLoopStart() {
  auto shutdown_cb = base::BindOnce([] {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Force a crash so that a crash report is generated.
    LOG(FATAL) << "Wayland protocol error.";
#else
    chrome::SessionEnding();
#endif
  });
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    ui::OzonePlatform::GetInstance()->PostMainMessageLoopStart(
        std::move(shutdown_cb));
    return;
  }
#endif
#if defined(USE_X11)
  x11::Connection::Get()->SetIOErrorHandler(std::move(shutdown_cb));
#endif
}

void ChromeBrowserMainExtraPartsOzone::PostMainMessageLoopRun() {
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    ui::OzonePlatform::GetInstance()->PostMainMessageLoopRun();
    return;
  }
#endif
}
