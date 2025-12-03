// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"

#include "chrome/common/buildflags.h"
#include "components/compose/buildflags.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/safe_browsing/buildflags.h"
#include "components/services/on_device_translation/buildflags/buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "chrome/browser/resources/certificate_manager/certificate_manager.mojom.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_ui.h"
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/ui/webui/compose/compose_untrusted_ui.h"
#include "chrome/common/compose/compose.mojom.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/signin/batch_upload/batch_upload.mojom.h"
#include "chrome/browser/ui/webui/signin/batch_upload_ui.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation.mojom.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "components/sync/base/features.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_ui.h"
#endif

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/fre/glic_fre_ui.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "content/public/browser/render_process_host.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/webui/tab_strip/tab_strip.mojom.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "components/browser_apis/tab_strip/tab_strip_api.mojom.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_ui.h"
#endif

#if BUILDFLAG(ENTERPRISE_WATERMARK)
#include "chrome/browser/ui/webui/watermark/watermark_ui.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/browser/ui/webui/reset_password/reset_password_ui.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

void PopulateChromeWebUIFrameBindersPartsFeatures(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  RegisterWebUIControllerInterfaceBinder<
      certificate_manager::mojom::CertificateManagerPageHandlerFactory,
      CertificateManagerUI>(map);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  RegisterWebUIControllerInterfaceBinder<
      batch_upload::mojom::PageHandlerFactory, BatchUploadUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      signout_confirmation::mojom::PageHandlerFactory, SignoutConfirmationUI>(
      map);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  RegisterWebUIControllerInterfaceBinder<
      zero_state_promo::mojom::PageHandlerFactory,
      extensions::ZeroStatePromoController>(map);
  RegisterWebUIControllerInterfaceBinder<
      custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory,
      extensions::ZeroStatePromoController>(map);
#endif

#if BUILDFLAG(ENABLE_GLIC)
  if (glic::GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext()))) {
    // Register binders for all eligible profiles.
    if (glic::GlicEnabling::IsUnifiedFreEnabled(Profile::FromBrowserContext(
            render_frame_host->GetProcess()->GetBrowserContext()))) {
      RegisterWebUIControllerInterfaceBinder<glic::mojom::FrePageHandlerFactory,
                                             glic::GlicUI>(map);
    } else {
      RegisterWebUIControllerInterfaceBinder<glic::mojom::FrePageHandlerFactory,
                                             glic::GlicFreUI>(map);
    }
    // For GlicUI, the WebUI page will check whether Glic is policy-enabled and
    // restrict access if needed. This isn't required for the GlicFreUI.
    RegisterWebUIControllerInterfaceBinder<glic::mojom::PageHandlerFactory,
                                           glic::GlicUI>(map);
  }
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  RegisterWebUIControllerInterfaceBinder<tab_strip::mojom::PageHandlerFactory,
                                         TabStripUI>(map);
  RegisterWebUIControllerInterfaceBinder<tabs_api::mojom::TabStripService,
                                         TabStripUI>(map);
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  RegisterWebUIControllerInterfaceBinder<
      tab_strip_internals::mojom::PageHandlerFactory, TabStripInternalsUI>(map);
#endif

#if BUILDFLAG(ENTERPRISE_WATERMARK)
  RegisterWebUIControllerInterfaceBinder<watermark::mojom::PageHandlerFactory,
                                         WatermarkUI>(map);
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  RegisterWebUIControllerInterfaceBinder<::mojom::ResetPasswordHandler,
                                         ResetPasswordUI>(map);
#endif
}

void PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsFeatures(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
#if BUILDFLAG(ENABLE_COMPOSE)
  registry.ForWebUI<ComposeUntrustedUI>()
      .Add<compose::mojom::ComposeSessionUntrustedPageHandlerFactory>();
#endif  // BUILDFLAG(ENABLE_COMPOSE)
}

}  // namespace chrome::internal
