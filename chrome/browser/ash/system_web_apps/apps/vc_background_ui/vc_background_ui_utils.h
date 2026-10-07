// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_UTILS_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/web_ui_controller.h"
#include "url/gurl.h"

class ApplicationLocaleStorage;

namespace content {
class WebUI;
}

namespace manta {
class MantaService;
}  // namespace manta

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash::vc_background_ui {

// `application_locale_storage` must not be null and must outlive the returned
// `WebUIController`.
// `shared_url_loader_factory` must not be null.
// `manta_service` is the profile's MantaService, resolved by the caller and
// injected into the SeaPen fetcher; it may be null.
std::unique_ptr<content::WebUIController> CreateVcBackgroundUI(
    const ApplicationLocaleStorage* application_locale_storage,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    content::WebUI* web_ui,
    const GURL& url,
    manta::MantaService* manta_service);
}  // namespace ash::vc_background_ui

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_UTILS_H_
