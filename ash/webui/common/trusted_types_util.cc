// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/common/trusted_types_util.h"

#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace ash {

void EnableTrustedTypesCSP(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'script';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types parse-html-subset sanitize-inner-html static-types "
      "xml-policy "
      // Add TrustedTypes policies for cr-lottie.
      "lottie-worker-script-loader "
      // Add TrustedTypes policy for <cros-lottie-renderer>.
      "cros-lottie-worker-script-loader "
      // Add TrustedTypes policies necessary for using CrOS' Lit bundle.
      "lit-html "
      // Add TrustedTypes policies necessary for using Desktop's Lit bundle.
      "lit-html-desktop "
      // Add TrustedTypes policies used during tests.
      "webui-test-script webui-test-html "
      // Add TrustedTypes policy used during Ash WebUI tests created in
      // `//ash/webui/common/trusted_types_test_util.h`.
      "ash-webui-test-script "
      // Add deprecated policies for Ash WebUIs using js.
      "ash-deprecated-sanitize-inner-html ash-deprecated-parse-html-subset "
      // Add TrustedTypes policy for creating the PDF plugin.
      "print-preview-plugin-loader "
      // Add TrustedTypes policies necessary for using Polymer.
      "polymer-html-literal polymer-template-event-attribute-policy "
      // Add TrustedTypes policies for Google Analytics and video processor
      // script URLs. Used by the Camera App.
      "ga-js-static video-processor-js-static "
      // Added TrustedTypes policy for trusted script URLs.
      "camera-app-trusted-script file-manager-trusted-script;");
}

}  // namespace ash
