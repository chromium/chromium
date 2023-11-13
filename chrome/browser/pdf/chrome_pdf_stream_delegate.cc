// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"

#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/pdf_resources.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/constants.h"
#include "pdf/pdf_features.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/common/webui_url_constants.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

namespace {

// Determines whether the PDF viewer should use Skia renderer based on the
// user's choice, the enterprise policy and the finch experiment. The priority
// hierarchy is: enterprise policy > user choice > finch experiment.
bool ShouldEnableSkiaRenderer(content::WebContents* contents) {
  CHECK(contents);
  const PrefService* prefs =
      Profile::FromBrowserContext(contents->GetBrowserContext())->GetPrefs();

  // When the enterprise policy is set.
  if (prefs->IsManagedPreference(prefs::kPdfUseSkiaRendererEnabled)) {
    return prefs->GetBoolean(prefs::kPdfUseSkiaRendererEnabled);
  }

  //  When the enterprise policy is not set, use finch/feature flag choice.
  return base::FeatureList::IsEnabled(
      chrome_pdf::features::kPdfUseSkiaRenderer);
}

// Associates a `pdf::PdfStreamDelegate::StreamInfo` with the PDF extension's
// or Print Preview's `blink::Document`.
// `ChromePdfStreamDelegate::MapToOriginalUrl()` initializes this in
// `PdfNavigationThrottle`, and then `ChromePdfStreamDelegate::GetStreamInfo()`
// returns the stashed result to `PdfURLLoaderRequestInterceptor`.
class StreamInfoHelper : public content::DocumentUserData<StreamInfoHelper> {
 public:
  absl::optional<pdf::PdfStreamDelegate::StreamInfo> TakeStreamInfo() {
    return std::move(stream_info_);
  }

 private:
  friend class content::DocumentUserData<StreamInfoHelper>;
  DOCUMENT_USER_DATA_KEY_DECL();

  StreamInfoHelper(content::RenderFrameHost* embedder_frame,
                   pdf::PdfStreamDelegate::StreamInfo stream_info)
      : content::DocumentUserData<StreamInfoHelper>(embedder_frame),
        stream_info_(std::move(stream_info)) {}

  absl::optional<pdf::PdfStreamDelegate::StreamInfo> stream_info_;
};

DOCUMENT_USER_DATA_KEY_IMPL(StreamInfoHelper);

}  // namespace

ChromePdfStreamDelegate::ChromePdfStreamDelegate() = default;
ChromePdfStreamDelegate::~ChromePdfStreamDelegate() = default;

absl::optional<GURL> ChromePdfStreamDelegate::MapToOriginalUrl(
    content::NavigationHandle& navigation_handle) {
  // The embedder frame's `Document` is used to store `StreamInfoHelper`.
  content::RenderFrameHost* embedder_frame = navigation_handle.GetParentFrame();

  StreamInfoHelper* helper =
      StreamInfoHelper::GetForCurrentDocument(embedder_frame);
  if (helper) {
    // PDF viewer and Print Preview only do this once per `blink::Document`.
    return absl::nullopt;
  }

  GURL original_url;
  StreamInfo info;

  content::WebContents* contents = navigation_handle.GetWebContents();
  base::WeakPtr<extensions::StreamContainer> stream;
  content::RenderFrameHost* embedder_parent_frame = embedder_frame->GetParent();
  if (base::FeatureList::IsEnabled(chrome_pdf::features::kPdfOopif)) {
    if (embedder_parent_frame) {
      // For the PDF viewer, the `embedder_frame` is the PDF extension frame.
      // The `StreamContainer` is stored using the PDF viewer's embedder frame,
      // which is the parent of the extension frame.
      auto* pdf_viewer_stream_manager =
          pdf::PdfViewerStreamManager::FromWebContents(contents);
      if (pdf_viewer_stream_manager) {
        stream = pdf_viewer_stream_manager->GetStreamContainer(
            embedder_parent_frame);
      }
    }
  } else {
    extensions::MimeHandlerViewGuest* guest =
        extensions::MimeHandlerViewGuest::FromWebContents(contents);
    if (guest) {
      stream = guest->GetStreamWeakPtr();
    }
  }

  const GURL& stream_url = navigation_handle.GetURL();
  if (stream) {
    if (stream->extension_id() != extension_misc::kPdfExtensionId ||
        stream->stream_url() != stream_url ||
        !stream->pdf_plugin_attributes()) {
      return absl::nullopt;
    }

    CHECK_EQ(embedder_frame->GetLastCommittedURL().host(),
             extension_misc::kPdfExtensionId);

    original_url = stream->original_url();
    info.background_color = base::checked_cast<SkColor>(
        stream->pdf_plugin_attributes()->background_color);
    info.full_frame = !stream->embedded();
    info.allow_javascript = stream->pdf_plugin_attributes()->allow_javascript;
    info.use_skia = ShouldEnableSkiaRenderer(contents);
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  } else if (stream_url.GetWithEmptyPath() ==
             chrome::kChromeUIUntrustedPrintURL) {
    CHECK_EQ(embedder_frame->GetLastCommittedURL().host(),
             chrome::kChromeUIPrintHost);

    // Print Preview doesn't have access to `chrome.mimeHandlerPrivate`, so just
    // use values that match those set by `PDFViewerPPElement`.
    original_url = stream_url;
    info.background_color = gfx::kGoogleGrey300;
    info.full_frame = false;
    info.allow_javascript = false;
    info.use_skia = ShouldEnableSkiaRenderer(contents);
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
  } else {
    return absl::nullopt;
  }

  static const base::NoDestructor<std::string> injected_script(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_PDF_PDF_INTERNAL_PLUGIN_WRAPPER_ROLLUP_JS));

  info.stream_url = stream_url;
  info.original_url = original_url;
  info.injected_script = injected_script.get();

  StreamInfoHelper::CreateForCurrentDocument(embedder_frame, std::move(info));
  return original_url;
}

absl::optional<pdf::PdfStreamDelegate::StreamInfo>
ChromePdfStreamDelegate::GetStreamInfo(
    content::RenderFrameHost* embedder_frame) {
  if (!embedder_frame) {
    return absl::nullopt;
  }

  StreamInfoHelper* helper =
      StreamInfoHelper::GetForCurrentDocument(embedder_frame);
  if (!helper) {
    return absl::nullopt;
  }

  // Only the call immediately following `MapToOriginalUrl()` requires a valid
  // `StreamInfo`; subsequent calls should just get nothing.
  return helper->TakeStreamInfo();
}
