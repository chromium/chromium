// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/aw_render_frame_ext.h"

#include <memory>
#include <string_view>
#include <utility>

#include "android_webview/common/aw_features.h"
#include "android_webview/common/mojom/frame.mojom.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/content_capture/common/content_capture_features.h"
#include "components/content_capture/renderer/content_capture_sender.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_hit_test_result.h"
#include "third_party/blink/public/web/web_image_cache.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_meaningful_layout.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"
#include "url/url_canon.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace android_webview {

namespace {

using autofill::AutofillAgent;
using ExtractAllDatalists = autofill::AutofillAgent::ExtractAllDatalists;
using FocusRequiresScroll = autofill::AutofillAgent::FocusRequiresScroll;
using QueryPasswordSuggestions =
    autofill::AutofillAgent::QueryPasswordSuggestions;
using SecureContextRequired = autofill::AutofillAgent::SecureContextRequired;
using UserGestureRequired = autofill::AutofillAgent::UserGestureRequired;
using UsesKeyboardAccessoryForSuggestions =
    autofill::AutofillAgent::UsesKeyboardAccessoryForSuggestions;
using EnableHeavyFormDataScraping =
    autofill::PasswordAutofillAgent::EnableHeavyFormDataScraping;

constexpr char kAddressPrefix[] = "geo:0,0?q=";
constexpr char kEmailPrefix[] = "mailto:";
constexpr char kPhoneNumberPrefix[] = "tel:";

GURL GetAbsoluteUrl(const blink::WebNode& node,
                    const std::u16string& url_fragment) {
  return GURL(node.GetDocument().CompleteURL(
      blink::WebString::FromUTF16(url_fragment)));
}

std::u16string GetHref(const blink::WebElement& element) {
  // Get the actual 'href' attribute, which might relative if valid or can
  // possibly contain garbage otherwise, so not using absoluteLinkURL here.
  return element.GetAttribute("href").Utf16();
}

GURL GetAbsoluteSrcUrl(const blink::WebElement& element) {
  if (element.IsNull())
    return GURL();
  return GetAbsoluteUrl(element, element.GetAttribute("src").Utf16());
}

blink::WebElement GetImgChild(const blink::WebNode& node) {
  // This implementation is incomplete (for example if is an area tag) but
  // matches the original WebViewClassic implementation.

  blink::WebElementCollection collection = node.GetElementsByHTMLTagName("img");
  DCHECK(!collection.IsNull());
  return collection.FirstItem();
}

GURL GetChildImageUrlFromElement(const blink::WebElement& element) {
  const blink::WebElement child_img = GetImgChild(element);
  if (child_img.IsNull())
    return GURL();
  return GetAbsoluteSrcUrl(child_img);
}

bool RemovePrefixAndAssignIfMatches(std::string_view prefix,
                                    const GURL& url,
                                    std::string* dest) {
  const std::string_view spec(url.possibly_invalid_spec());

  if (base::StartsWith(spec, prefix)) {
    url::RawCanonOutputW<1024> output;
    url::DecodeURLEscapeSequences(spec.substr(prefix.length()),
                                  url::DecodeURLMode::kUTF8OrIsomorphic,
                                  &output);
    *dest = base::UTF16ToUTF8(output.view());
    return true;
  }
  return false;
}

void DistinguishAndAssignSrcLinkType(const GURL& url,
                                     mojom::HitTestData* data) {
  if (RemovePrefixAndAssignIfMatches(kAddressPrefix, url,
                                     &data->extra_data_for_type)) {
    data->type = mojom::HitTestDataType::kGeo;
  } else if (RemovePrefixAndAssignIfMatches(kPhoneNumberPrefix, url,
                                            &data->extra_data_for_type)) {
    data->type = mojom::HitTestDataType::kPhone;
  } else if (RemovePrefixAndAssignIfMatches(kEmailPrefix, url,
                                            &data->extra_data_for_type)) {
    data->type = mojom::HitTestDataType::kEmail;
  } else {
    data->type = mojom::HitTestDataType::kSrcLink;
    data->extra_data_for_type = url.possibly_invalid_spec();
    if (!data->extra_data_for_type.empty())
      data->href = base::UTF8ToUTF16(data->extra_data_for_type);
  }
}

void PopulateHitTestData(const GURL& absolute_link_url,
                         const GURL& absolute_image_url,
                         bool is_editable,
                         mojom::HitTestData* data) {
  // Note: Using GURL::is_empty instead of GURL:is_valid due to the
  // WebViewClassic allowing any kind of protocol which GURL::is_valid
  // disallows. Similar reasons for using GURL::possibly_invalid_spec instead of
  // GURL::spec.
  if (!absolute_image_url.is_empty())
    data->img_src = absolute_image_url;

  const bool is_javascript_scheme =
      absolute_link_url.SchemeIs(url::kJavaScriptScheme);
  const bool has_link_url = !absolute_link_url.is_empty();
  const bool has_image_url = !absolute_image_url.is_empty();

  if (has_link_url && !has_image_url && !is_javascript_scheme) {
    DistinguishAndAssignSrcLinkType(absolute_link_url, data);
  } else if (has_link_url && has_image_url && !is_javascript_scheme) {
    data->type = mojom::HitTestDataType::kSrcImageLink;
    data->extra_data_for_type = data->img_src.possibly_invalid_spec();
    if (absolute_link_url.is_valid())
      data->href = base::UTF8ToUTF16(absolute_link_url.possibly_invalid_spec());
  } else if (!has_link_url && has_image_url) {
    data->type = mojom::HitTestDataType::kImage;
    data->extra_data_for_type = data->img_src.possibly_invalid_spec();
  } else if (is_editable) {
    data->type = mojom::HitTestDataType::kEditText;
    DCHECK_EQ(0u, data->extra_data_for_type.length());
  }
}

}  // namespace

AwRenderFrameExt::AwRenderFrameExt(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
  auto password_autofill_agent =
      std::make_unique<autofill::PasswordAutofillAgent>(
          render_frame, &registry_, EnableHeavyFormDataScraping(false));
  new AutofillAgent(
      render_frame,
      {ExtractAllDatalists(true), FocusRequiresScroll(false),
       QueryPasswordSuggestions(true), SecureContextRequired(true),
       UserGestureRequired(false), UsesKeyboardAccessoryForSuggestions(false)},
      std::move(password_autofill_agent), nullptr, &registry_);
  if (content_capture::features::IsContentCaptureEnabled())
    new content_capture::ContentCaptureSender(render_frame, &registry_);

  // If we are the main frame register an additional mojo interface.
  if (render_frame->IsMainFrame()) {
    registry_.AddInterface<mojom::LocalMainFrame>(base::BindRepeating(
        &AwRenderFrameExt::BindLocalMainFrame, base::Unretained(this)));
  }
}

AwRenderFrameExt::~AwRenderFrameExt() = default;

const mojo::AssociatedRemote<mojom::FrameHost>&
AwRenderFrameExt::GetFrameHost() {
  if (!frame_host_remote_) {
    render_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
        &frame_host_remote_);
  }

  return frame_host_remote_;
}

bool AwRenderFrameExt::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return registry_.TryBindInterface(interface_name, handle);
}

void AwRenderFrameExt::DidCreateDocumentElement() {
  if (!base::FeatureList::IsEnabled(
          features::kWebViewHitTestInBlinkOnTouchStart)) {
    return;
  }
  render_frame()->GetWebFrame()->AddHitTestOnTouchStartCallback(
      base::BindRepeating(&AwRenderFrameExt::HandleHitTestResult,
                          base::Unretained(this)));
}

void AwRenderFrameExt::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  // Clear the cache when we cross site boundaries in the main frame.
  //
  // We're trying to approximate what happens with a multi-process Chromium,
  // where navigation across origins would cause a new render process to spin
  // up, and thus start with a clear cache. Wiring up a signal from browser to
  // renderer code to say "this navigation would have switched processes" would
  // be disruptive, so this clearing of the cache is the compromise.
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  if (!frame->Parent()) {
    url::Origin new_origin = url::Origin::Create(frame->GetDocument().Url());
    if (!new_origin.IsSameOriginWith(last_origin_)) {
      last_origin_ = new_origin;
      blink::WebImageCache::Clear();
    }
  }
}

void AwRenderFrameExt::FocusedElementChanged(const blink::WebElement& element) {
  if (element.IsNull() || !render_frame())
    return;

  auto data = mojom::HitTestData::New();

  data->href = GetHref(element);
  data->anchor_text = element.TextContent().Utf16();

  GURL absolute_link_url;
  if (element.IsLink())
    absolute_link_url = GetAbsoluteUrl(element, data->href);

  GURL absolute_image_url = GetChildImageUrlFromElement(element);

  PopulateHitTestData(absolute_link_url, absolute_image_url,
                      element.IsEditable(), data.get());

  GetFrameHost()->UpdateHitTestData(std::move(data));
}

// Only main frame needs to *receive* the hit test request, because all we need
// is to get the blink::webView object and invoke a the hitTestResultForTap API
// from it.
void AwRenderFrameExt::HitTest(const gfx::PointF& touch_center,
                               const gfx::SizeF& touch_area) {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  const blink::WebHitTestResult result = webview->HitTestResultForTap(
      gfx::Point(touch_center.x(), touch_center.y()),
      gfx::Size(touch_area.width(), touch_area.height()));
  HandleHitTestResult(result);
}

void AwRenderFrameExt::HandleHitTestResult(
    const blink::WebHitTestResult& result) {
  auto data = mojom::HitTestData::New();

  GURL absolute_image_url = result.AbsoluteImageURL();
  if (!result.UrlElement().IsNull()) {
    data->anchor_text = result.UrlElement().TextContent().Utf16();
    data->href = GetHref(result.UrlElement());
    // If we hit an image that failed to load, Blink won't give us its URL.
    // Fall back to walking the DOM in this case.
    if (absolute_image_url.is_empty())
      absolute_image_url = GetChildImageUrlFromElement(result.UrlElement());
  }

  PopulateHitTestData(result.AbsoluteLinkURL(), absolute_image_url,
                      result.IsContentEditable(), data.get());

  GetFrameHost()->UpdateHitTestData(std::move(data));
}

void AwRenderFrameExt::SetInitialPageScale(double page_scale_factor) {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  webview->SetInitialPageScaleOverride(page_scale_factor);
}

void AwRenderFrameExt::SetTextZoomFactor(float zoom_factor) {
  // TODO(crbug.com/40132194): This will need to be set on every local root
  // when site isolation is used in android webview.
  DCHECK(render_frame()->IsMainFrame());

  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  // Hide selection and autofill popups.
  webview->CancelPagePopup();

  render_frame()->GetWebFrame()->FrameWidget()->SetTextZoomFactor(zoom_factor);
}

void AwRenderFrameExt::DocumentHasImage(DocumentHasImageCallback callback) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  // DocumentHasImages Mojo message should only be sent to the outermost main
  // frame.
  DCHECK(frame);
  DCHECK(frame->IsOutermostMainFrame());

  const blink::WebElement child_img = GetImgChild(frame->GetDocument());
  bool has_images = !child_img.IsNull();

  std::move(callback).Run(has_images);
}

void AwRenderFrameExt::SmoothScroll(int32_t target_x,
                                    int32_t target_y,
                                    base::TimeDelta duration) {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  webview->SmoothScroll(target_x, target_y, duration);
}

void AwRenderFrameExt::ResetScrollAndScaleState() {
  blink::WebView* webview = GetWebView();
  if (!webview)
    return;

  webview->ResetScrollAndScaleState();
}

blink::WebView* AwRenderFrameExt::GetWebView() {
  if (!render_frame())
    return nullptr;

  return render_frame()->GetWebView();
}

blink::WebFrameWidget* AwRenderFrameExt::GetWebFrameWidget() {
  return render_frame()
             ? render_frame()->GetWebFrame()->LocalRoot()->FrameWidget()
             : nullptr;
}

void AwRenderFrameExt::OnDestruct() {
  delete this;
}

void AwRenderFrameExt::BindLocalMainFrame(
    mojo::PendingAssociatedReceiver<mojom::LocalMainFrame> pending_receiver) {
  // When bfcache is enabled, this bind can occur multiple times.
  // receiver should be reset before.
  local_main_frame_receiver_.reset();
  local_main_frame_receiver_.Bind(std::move(pending_receiver));
}

}  // namespace android_webview
