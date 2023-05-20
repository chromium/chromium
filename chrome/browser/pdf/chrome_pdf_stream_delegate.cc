// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/chrome_pdf_stream_delegate.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/pdf/pdf_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/pdf_resources.h"
#include "components/pdf/browser/pdf_stream_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents_user_data.h"
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

// Associates a `pdf::PdfStreamDelegate::StreamInfo` with a `WebContents`.
// `ChromePdfStreamDelegate::MapToOriginalUrl()` initializes this in
// `PdfNavigationThrottle`, and then `ChromePdfStreamDelegate::GetStreamInfo()`
// returns the stashed result to `PdfURLLoaderRequestInterceptor`.
class StreamInfoHelper : public content::WebContentsUserData<StreamInfoHelper> {
 public:
  absl::optional<pdf::PdfStreamDelegate::StreamInfo> TakeStreamInfo() {
    return std::move(stream_info_);
  }

 private:
  friend class content::WebContentsUserData<StreamInfoHelper>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  StreamInfoHelper(content::WebContents* contents,
                   pdf::PdfStreamDelegate::StreamInfo stream_info)
      : content::WebContentsUserData<StreamInfoHelper>(*contents),
        stream_info_(std::move(stream_info)) {}

  absl::optional<pdf::PdfStreamDelegate::StreamInfo> stream_info_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(StreamInfoHelper);

}  // namespace

ChromePdfStreamDelegate::ChromePdfStreamDelegate() = default;
ChromePdfStreamDelegate::~ChromePdfStreamDelegate() = default;

absl::optional<GURL> ChromePdfStreamDelegate::MapToOriginalUrl(
    content::WebContents* contents,
    const GURL& stream_url) {
  StreamInfoHelper* helper = StreamInfoHelper::FromWebContents(contents);
  if (helper) {
    // PDF viewer and Print Preview only do this once per WebContents.
    return absl::nullopt;
  }

  GURL original_url;
  StreamInfo info;

  extensions::MimeHandlerViewGuest* guest =
      extensions::MimeHandlerViewGuest::FromWebContents(contents);
  if (guest) {
    base::WeakPtr<extensions::StreamContainer> stream =
        guest->GetStreamWeakPtr();
    if (!stream || stream->extension_id() != extension_misc::kPdfExtensionId ||
        stream->stream_url() != stream_url ||
        !stream->pdf_plugin_attributes()) {
      return absl::nullopt;
    }

    original_url = stream->original_url();
    info.background_color = base::checked_cast<SkColor>(
        stream->pdf_plugin_attributes()->background_color);
    info.full_frame = !stream->embedded();
    info.allow_javascript = stream->pdf_plugin_attributes()->allow_javascript;
    info.use_skia = ShouldEnableSkiaRenderer(contents);
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  } else if (stream_url.GetWithEmptyPath() ==
             chrome::kChromeUIUntrustedPrintURL) {
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
  StreamInfoHelper::CreateForWebContents(contents, std::move(info));
  return original_url;
}

absl::optional<pdf::PdfStreamDelegate::StreamInfo>
ChromePdfStreamDelegate::GetStreamInfo(content::WebContents* contents) {
  StreamInfoHelper* helper = StreamInfoHelper::FromWebContents(contents);
  if (!helper)
    return absl::nullopt;

  // Only the call immediately following `MapToOriginalUrl()` requires a valid
  // `StreamInfo`; subsequent calls should just get nothing.
  return helper->TakeStreamInfo();
}
