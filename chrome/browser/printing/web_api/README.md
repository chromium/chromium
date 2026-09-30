# Web Printing API (`chrome/browser/printing/web_api`)

This directory implements the browser-process Mojo service for the
[Web Printing API](https://github.com/WICG/web-printing), restricted to
Isolated Web Apps (IWAs) on ChromeOS.

## Companion Documentation

- **Core IWA Component:**
  [`//components/webapps/isolated_web_apps`](/components/webapps/isolated_web_apps/README.md)
- **Browser IWA Engine:**
  [`//chrome/browser/web_applications/isolated_web_apps`](/chrome/browser/web_applications/isolated_web_apps/README.md)

## Key Components

- `web_printing_service_binder.{h,cc}`: Entry point
  `CreateWebPrintingServiceForFrame()` verifying:
  1. `blink::features::kWebPrinting` is enabled.
  2. `PermissionsPolicyFeature::kWebPrinting` is enabled in the frame.
  3. `content::HasIsolatedContextCapability(render_frame_host)` is satisfied.
  4. On ChromeOS (with CUPS), the frame is associated with an installed web app
     (`web_app::WebAppTabHelper::GetAppId`).
- `WebPrintingServiceChromeOS`: Implements `blink::mojom::WebPrintingService`
  and `blink::mojom::WebPrinter` on ChromeOS—verifying
  `blink::PermissionType::WEB_PRINTING` via `content::PermissionController`,
  enumerating local CUPS printers (`ash::LocalPrinter`), fetching printer
  attributes (`chromeos::CupsWrapper`), flattening PDF blobs
  (`PdfBlobDataFlattener`), and submitting print jobs (`PrintJobController`).
- `InProgressJobsStorageChromeOS`: Implements
  `blink::mojom::WebPrintJobController` and `ash::CupsPrintJobManager::Observer`
  to track active print jobs, handle cancellation, and dispatch job-state
  updates to `blink::mojom::WebPrintJobStateObserver` Mojo remotes.
- `web_printing_mojom_traits.{h,cc}`, `web_printing_type_converters.{h,cc}`, &
  `web_printing_utils.{h,cc}`: Type-safe conversion between Blink Mojo print job
  structures and ChromeOS printing types (`ipc/SECURITY_OWNERS` reviewed) plus
  CUPS capability matching helpers (`FindAdvancedCapability`).
