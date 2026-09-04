// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/privileged_web_contents.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/pwc/pwc_api_binder.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/back_forward_cache/disabled_reason_id.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace pwc {

namespace {

// Marks a WebContents as owned by a PrivilegedWebContents and links back to
// its owner. Attached for the whole lifetime of the WebContents; the owner
// strictly outlives the WebContents, so the back-pointer never dangles.
class PrivilegedWebContentsHolder
    : public content::WebContentsUserData<PrivilegedWebContentsHolder> {
 public:
  ~PrivilegedWebContentsHolder() override = default;

  PrivilegedWebContents* privileged_web_contents() { return owner_; }

 private:
  friend class content::WebContentsUserData<PrivilegedWebContentsHolder>;

  PrivilegedWebContentsHolder(content::WebContents* web_contents,
                              PrivilegedWebContents* owner)
      : content::WebContentsUserData<PrivilegedWebContentsHolder>(
            *web_contents),
        owner_(owner) {}

  const raw_ptr<PrivilegedWebContents> owner_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrivilegedWebContentsHolder);

}  // namespace

// static
std::unique_ptr<PrivilegedWebContents> PrivilegedWebContents::Create(
    PrivilegedComponent component,
    content::BrowserContext* browser_context,
    std::unique_ptr<PwcPolicyDelegate> policy_delegate) {
  CHECK(base::FeatureList::IsEnabled(mojom::features::kPrivilegedWebContents));
  CHECK(browser_context);
  CHECK(policy_delegate);
  // Not std::make_unique: the constructor is private.
  return base::WrapUnique(new PrivilegedWebContents(
      component, browser_context, std::move(policy_delegate)));
}

// static
PrivilegedWebContents* PrivilegedWebContents::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  auto* holder = PrivilegedWebContentsHolder::FromWebContents(web_contents);
  return holder ? holder->privileged_web_contents() : nullptr;
}

PrivilegedWebContents::PrivilegedWebContents(
    PrivilegedComponent component,
    content::BrowserContext* browser_context,
    std::unique_ptr<PwcPolicyDelegate> policy_delegate)
    : policy_(component, std::move(policy_delegate)),
      bridge_(std::make_unique<PwcApiBinder>()) {
  // No StoragePartitionConfig override: the WebContents lives in the
  // profile's default partition so the component shares the live cookie jar.
  content::WebContents::CreateParams params(browser_context);
  content::WebContents::PrivilegedParams privileged_params;
  privileged_params.feature_id = policy_.content_feature_id();
  privileged_params.disallow_service_worker_control =
      policy_.disallow_service_worker_control();
  privileged_params.disallow_shared_workers = policy_.disallow_shared_workers();
  params.privileged_params = privileged_params;

  web_contents_ = content::WebContents::Create(params);
  PrivilegedWebContentsHolder::CreateForWebContents(web_contents_.get(), this);
  web_contents_->SetDelegate(this);
  Observe(web_contents_.get());
}

PrivilegedWebContents::~PrivilegedWebContents() {
  web_contents_->SetDelegate(nullptr);
  web_contents_.reset();
}

bool PrivilegedWebContents::EmbedderDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return false;
}

void PrivilegedWebContents::EmbedderDelegate::ContentsZoomChange(bool zoom_in) {
}

content::PreloadingEligibility PrivilegedWebContents::IsPrerender2Supported(
    content::WebContents& web_contents,
    content::PreloadingTriggerType trigger_type) {
  // Never prerender inside a PrivilegedWebContents. See the header for why.
  // Note this also matches the WebContentsDelegate default, but is stated
  // explicitly so the security property does not depend on the default.
  return content::PreloadingEligibility::kPreloadingUnsupportedByWebContents;
}

content::WebContents* PrivilegedWebContents::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // Related-window creation is denied for privileged content
  // (ChromeContentBrowserClient::CanCreateWindow), so a new WebContents should
  // never be handed to this delegate. Drop it loudly if it ever is.
  NOTREACHED();
}

bool PrivilegedWebContents::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (embedder_delegate_) {
    return embedder_delegate_->HandleKeyboardEvent(source, event);
  }
  return false;
}

void PrivilegedWebContents::ContentsZoomChange(bool zoom_in) {
  if (embedder_delegate_) {
    embedder_delegate_->ContentsZoomChange(zoom_in);
  }
}

void PrivilegedWebContents::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Keep every committed document out of the back-forward cache. Disabling the
  // primary main frame is sufficient: a page can only be cached if its main
  // frame and all subframes can. See the header for why. A same-document
  // navigation stays in the same RenderFrameHost, which is already disabled, so
  // there is nothing to re-disable.
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  content::BackForwardCache::DisableForRenderFrameHost(
      navigation_handle->GetRenderFrameHost(),
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kPrivilegedWebContents));
}

}  // namespace pwc
