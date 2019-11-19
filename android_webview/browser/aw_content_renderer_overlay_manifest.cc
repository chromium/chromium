// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_renderer_overlay_manifest.h"

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/no_destructor.h"
#include "components/autofill/content/common/mojom/autofill_agent.mojom.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "content/public/common/service_names.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace android_webview {

const service_manager::Manifest& GetAWContentRendererOverlayManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "browser",
              service_manager::Manifest::InterfaceList<
                  autofill::mojom::AutofillAgent,
                  autofill::mojom::PasswordAutofillAgent,
                  autofill::mojom::PasswordGenerationAgent,
                  safe_browsing::mojom::ThreatReporter,
                  mojom::JsJavaConfigurator, mojom::JsToJavaMessaging>())
          .Build()};
  return *manifest;
}

}  // namespace android_webview
