// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_extra_parts_ozone.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/ozone/public/ozone_platform.h"

ChromeBrowserMainExtraPartsOzone::ChromeBrowserMainExtraPartsOzone() = default;

ChromeBrowserMainExtraPartsOzone::~ChromeBrowserMainExtraPartsOzone() = default;

void ChromeBrowserMainExtraPartsOzone::PostCreateMainMessageLoop() {
  auto shutdown_cb = base::BindOnce([] {
    chrome::SessionEnding();
    LOG(FATAL) << "Browser failed to shutdown.";
  });
  ui::OzonePlatform::GetInstance()->PostCreateMainMessageLoop(
      std::move(shutdown_cb),
      content::GetUIThreadTaskRunner({content::BrowserTaskType::kUserInput}));
}

void ChromeBrowserMainExtraPartsOzone::PostMainMessageLoopRun() {
#if !BUILDFLAG(IS_CHROMEOS_LACROS) && !BUILDFLAG(IS_LINUX)
  // Lacros's `PostMainMessageLoopRun` must be called at the very end of
  // `PostMainMessageLoopRun` in
  // `ChromeBrowserMainPartsLacros::PostMainMessageLoopRun`.
  ui::OzonePlatform::GetInstance()->PostMainMessageLoopRun();
#endif
}
