// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_service_binder.h"

#include <utility>

#include "base/feature_list.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
#include "chrome/browser/printing/web_api/web_printing_service_chromeos.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#endif

namespace printing {

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
std::optional<std::string> InferAssociatedAppId(
    content::RenderFrameHost* render_frame_host) {
  if (auto* web_app_id = web_app::WebAppTabHelper::GetAppId(
          content::WebContents::FromRenderFrameHost(render_frame_host))) {
    // If this is a web app, return its id.
    return *web_app_id;
  }
  auto* extension =
      extensions::ExtensionRegistry::Get(render_frame_host->GetBrowserContext())
          ->enabled_extensions()
          .GetExtensionOrAppByURL(render_frame_host->GetLastCommittedURL());
  if (extension && extension->is_platform_app()) {
    // If this is a chrome app, return its id.
    return extension->id();
  }
  return std::nullopt;
}
#endif

void CreateWebPrintingServiceForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver) {
  if (!base::FeatureList::IsEnabled(blink::features::kWebPrinting)) {
    mojo::ReportBadMessage("The WebPrinting API is disabled.");
    return;
  }
  if (!render_frame_host->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kWebPrinting)) {
    mojo::ReportBadMessage(
        "Access to the feature \"web-printing\" is disallowed by permissions "
        "policy.");
    return;
  }
  // There are some security concerns around this API's fingerprinting surface
  // and the lack of motivating use cases outside of IWAs -- most users either
  // have no printers at all or only one which leads us to a 'take it or leave
  // it' situation where regular websites do not really need to access printer
  // information.
  // This decision might be reconsidered in the future, but for now we'll stick
  // to the IWA-only approach.
  if (!content::HasIsolatedContextCapability(render_frame_host)) {
    mojo::ReportBadMessage(
        "Frame is not sufficiently isolated to use the WebPrinting API.");
    return;
  }

#if BUILDFLAG(IS_CHROMEOS) && BUILDFLAG(USE_CUPS)
  std::optional<std::string> app_id = InferAssociatedAppId(render_frame_host);
  if (!app_id) {
    mojo::ReportBadMessage("Web Printing API is only available inside apps.");
    return;
  }
  // This class inherits from content::DocumentService<> -- its lifetime is
  // bound to the associated `render_frame_host`.
  new WebPrintingServiceChromeOS(render_frame_host, std::move(receiver),
                                 *app_id);
#else
  mojo::ReportBadMessage(
      "WebPrinting API is currently supported only on ChromeOS with CUPS "
      "enabled.");
#endif
}

}  // namespace printing
