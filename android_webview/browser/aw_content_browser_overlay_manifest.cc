// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_overlay_manifest.h"

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/no_destructor.h"
#include "components/autofill/content/common/mojom/autofill_driver.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace android_webview {

const service_manager::Manifest& GetAWContentBrowserOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeCapability("renderer",
                            service_manager::Manifest::InterfaceList<
                                safe_browsing::mojom::SafeBrowsing,
                                spellcheck::mojom::SpellCheckHost>())
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "renderer",
              service_manager::Manifest::InterfaceList<
                  autofill::mojom::AutofillDriver,
                  autofill::mojom::PasswordManagerDriver,
                  mojom::JsToJavaMessaging>())
          .Build()};
  return *manifest;
}

}  // namespace android_webview
