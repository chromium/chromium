// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"

namespace ash::shimless_rma {

ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult::
    PrepareDiagnosticsAppBrowserContextResult(
        const raw_ptr<content::BrowserContext>& context,
        const std::string& extension_id,
        const web_package::SignedWebBundleId& iwa_id,
        const std::string& name,
        const std::optional<std::string>& permission_message)
    : context(context),
      extension_id(extension_id),
      iwa_id(iwa_id),
      name(name),
      permission_message(permission_message) {}

ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult::
    PrepareDiagnosticsAppBrowserContextResult(
        const PrepareDiagnosticsAppBrowserContextResult&) = default;

ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult&
ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult::operator=(
    const PrepareDiagnosticsAppBrowserContextResult&) = default;

ShimlessRmaDelegate::PrepareDiagnosticsAppBrowserContextResult::
    ~PrepareDiagnosticsAppBrowserContextResult() = default;

}  // namespace ash::shimless_rma
