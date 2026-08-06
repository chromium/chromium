// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/privileged_web_contents.h"

#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

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
    : policy_(component, std::move(policy_delegate)) {
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
}

PrivilegedWebContents::~PrivilegedWebContents() {
  web_contents_->SetDelegate(nullptr);
  web_contents_.reset();
}

content::PreloadingEligibility PrivilegedWebContents::IsPrerender2Supported(
    content::WebContents& web_contents,
    content::PreloadingTriggerType trigger_type) {
  // Never prerender inside a PrivilegedWebContents. See the header for why.
  // Note this also matches the WebContentsDelegate default, but is stated
  // explicitly so the security property does not depend on the default.
  return content::PreloadingEligibility::kPreloadingUnsupportedByWebContents;
}

}  // namespace pwc
