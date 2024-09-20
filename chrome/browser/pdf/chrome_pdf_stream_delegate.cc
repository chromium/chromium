// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"

#include <optional>
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
#include "net/http/http_response_headers.h"
#include "pdf/pdf_features.h"
#include "printing/buildflags/buildflags.h"
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
  std::optional<pdf::PdfStreamDelegate::StreamInfo> TakeStreamInfo() {
    return std::move(stream_info_);
  }

 private:
  friend class content::DocumentUserData<StreamInfoHelper>;
  DOCUMENT_USER_DATA_KEY_DECL();

  StreamInfoHelper(content::RenderFrameHost* embedder_frame,
                   pdf::PdfStreamDelegate::StreamInfo stream_info)
      : content::DocumentUserData<StreamInfoHelper>(embedder_frame),
        stream_info_(std::move(stream_info)) {}

  std::optional<pdf::PdfStreamDelegate::StreamInfo> stream_info_;
};

DOCUMENT_USER_DATA_KEY_IMPL(StreamInfoHelper);

}  // namespace

ChromePdfStreamDelegate::ChromePdfStreamDelegate() = default;
ChromePdfStreamDelegate::~ChromePdfStreamDelegate() = default;

std::optional<GURL> ChromePdfStreamDelegate::MapToOriginalUrl(
    content::NavigationHandle& navigation_handle) {
  // The embedder frame's `Document` is used to store `StreamInfoHelper`.
  content::RenderFrameHost* embedder_frame = navigation_handle.GetParentFrame();

  StreamInfoHelper* helper =
      StreamInfoHelper::GetForCurrentDocument(embedder_frame);
  if (helper) {
    // PDF viewer and Print Preview only do this once per `blink::Document`.
    return std::nullopt;
  }

  GURL original_url;
  StreamInfo info;

  content::WebContents* contents = navigation_handle.GetWebContents();
  base::WeakPtr<extensions::StreamContainer> stream;
  content::RenderFrameHost* embedder_parent_frame = embedder_frame->GetParent();
  if (chrome_pdf::features::IsOopifPdfEnabled()) {
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
        extensions::MimeHandlerViewGuest::FromNavigationHandle(
            &navigation_handle);
    if (guest) {
      stream = guest->GetStreamWeakPtr();
    }
  }

  const GURL& stream_url = navigation_handle.GetURL();
  if (stream) {
    if (stream->extension_id() != extension_misc::kPdfExtensionId ||
        stream->stream_url() != stream_url ||
        !stream->pdf_plugin_attributes()) {
      return std::nullopt;
    }

    CHECK_EQ(embedder_frame->GetLastCommittedURL().host(),
             extension_misc::kPdfExtensionId);

    original_url = stream->original_url();
    info.background_color = base::checked_cast<SkColor>(
        stream->pdf_plugin_attributes()->background_color);
    info.full_frame = !stream->embedded();
    info.allow_javascript = stream->pdf_plugin_attributes()->allow_javascript;
    info.use_skia = ShouldEnableSkiaRenderer(contents);
    if (chrome_pdf::features::IsOopifPdfEnabled()) {
      net::HttpResponseHeaders* response_headers = stream->response_headers();
      info.require_corp = response_headers &&
                          response_headers->HasHeaderValue(
                              "Cross-Origin-Embedder-Policy", "require-corp");
    }
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
    return std::nullopt;
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

std::optional<pdf::PdfStreamDelegate::StreamInfo>
ChromePdfStreamDelegate::GetStreamInfo(
    content::RenderFrameHost* embedder_frame) {
  if (!embedder_frame) {
    return std::nullopt;
  }

  StreamInfoHelper* helper =
      StreamInfoHelper::GetForCurrentDocument(embedder_frame);
  if (!helper) {
    return std::nullopt;
  }

  // Only the call immediately following `MapToOriginalUrl()` requires a valid
  // `StreamInfo`; subsequent calls should just get nothing.
  return helper->TakeStreamInfo();
}

void ChromePdfStreamDelegate::OnPdfEmbedderSandboxed(
    content::FrameTreeNodeId frame_tree_node_id) {
  // Clean up the stream for a sandboxed embedder frame, as sandboxed frames
  // should be unable to instantiate the PDF viewer.
  CHECK(chrome_pdf::features::IsOopifPdfEnabled());

  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents) {
    return;
  }

  auto* pdf_viewer_stream_manager =
      pdf::PdfViewerStreamManager::FromWebContents(web_contents);
  if (!pdf_viewer_stream_manager) {
    return;
  }

  // The stream should always be unclaimed, since the navigation hasn't
  // committed.
  pdf_viewer_stream_manager->DeleteUnclaimedStreamInfo(frame_tree_node_id);
}

bool ChromePdfStreamDelegate::ShouldAllowPdfFrameNavigation(
    content::NavigationHandle* navigation_handle) {
  // Blocks any non-setup navigations in the PDF extension frame and the PDF
  // content frame.

  // OOPIF PDF viewer only.
  if (!chrome_pdf::features::IsOopifPdfEnabled()) {
    return true;
  }

  auto* pdf_viewer_stream_manager =
      pdf::PdfViewerStreamManager::FromWebContents(
          navigation_handle->GetWebContents());
  if (!pdf_viewer_stream_manager) {
    return true;
  }

  // The parent frame should always exist after main frame navigations are
  // filtered out in `PdfNavigationThrottle::WillStartRequest()`. The
  // parent frame could be the PDF extension frame, PDF embedder frame, or an
  // unrelated frame.
  content::RenderFrameHost* parent_frame = navigation_handle->GetParentFrame();
  CHECK(parent_frame);

  const GURL& url = navigation_handle->GetURL();

  // If `parent_frame` is the PDF embedder frame and thus has an
  // `extensions::StreamContainer`, then the current frame could be the PDF
  // extension frame.
  base::WeakPtr<extensions::StreamContainer> stream =
      pdf_viewer_stream_manager->GetStreamContainer(parent_frame);
  content::FrameTreeNodeId frame_tree_node_id =
      navigation_handle->GetFrameTreeNodeId();
  if (stream) {
    // Allow navigations for unrelated frames, which might be injected by
    // unrelated extensions. Only allow the PDF extension frame to navigate to
    // the extension URL once.
    return !pdf_viewer_stream_manager->IsPdfExtensionFrameTreeNodeId(
               parent_frame, frame_tree_node_id) ||
           (!pdf_viewer_stream_manager->DidPdfExtensionFinishNavigation(
                parent_frame) &&
            url == stream->handler_url());
  }

  // If this navigation is for a PDF content frame, then there should be a
  // grandparent frame (the PDF embedder frame) with a stream container. If this
  // navigation is unrelated to PDFs, then there may or may not be a grandparent
  // frame, and there will not be a stream container. In that case, the
  // navigation should not be blocked.
  content::RenderFrameHost* grandparent_frame = parent_frame->GetParent();
  if (!grandparent_frame) {
    return true;
  }
  stream = pdf_viewer_stream_manager->GetStreamContainer(grandparent_frame);
  if (!stream) {
    return true;
  }

  // Allow navigations for unrelated frames, which might be injected by
  // unrelated extensions. Only allow the PDF content frame to navigate to the
  // original PDF URL once.
  return !pdf_viewer_stream_manager->IsPdfContentFrameTreeNodeId(
             grandparent_frame, frame_tree_node_id) ||
         (!pdf_viewer_stream_manager->DidPdfContentNavigate(
              grandparent_frame) &&
          url == stream->original_url());
}
