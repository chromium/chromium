// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_UTILS_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_UTILS_H_

#include <memory>

#include "content/public/browser/web_ui_controller.h"
#include "url/gurl.h"

namespace content {
class WebUI;
}

namespace ash::vc_background_ui {

std::unique_ptr<content::WebUIController> CreateVcBackgroundUI(
    content::WebUI* web_ui,
    const GURL& url);

}

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_UTILS_H_
