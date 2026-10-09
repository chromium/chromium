// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/page_context_util.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/common/webui_url_constants.h"
#include "components/actor/core/safety_list_manager.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace ttc {

namespace {

// Returns whether `url`'s scheme and host are supported for page context.
// Similar to, but stricter than, Glic's IsTabValidForSharing() (see
// chrome/browser/glic/host/context/glic_sharing_utils.cc), which also allows
// file: URLs and a few other WebUI pages.
// TODO(b/555804152): On Android the New Tab Page is native
// (chrome-native://newtab/) and so is not supported here. Decide whether it
// should be.
bool IsSchemeAndHostSupported(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme)) {
    const std::string_view host = url.host();
    if (host == chrome::kChromeUINewTabHost ||
        host == chrome::kChromeUINewTabPageHost ||
        host == chrome::kChromeUINewTabPageThirdPartyHost) {
      return true;
    }
  }
  // Excludes all other chrome:// URLs, as well as file:, javascript:, about:,
  // data:, blob:, chrome-extension:, etc.
  return url.SchemeIsHTTPOrHTTPS();
}

}  // namespace

void IsUrlSupportedForPageContext(const GURL& url,
                                  base::OnceCallback<void(bool)> callback) {
  if (!IsSchemeAndHostSupported(url)) {
    std::move(callback).Run(false);
    return;
  }

  // Ask the actor safety lists whether a navigation from `url` to itself is
  // allowed, and invoke `callback` with false iff it is blocked. This mirrors
  // how the actor's safety list gating predicate (see
  // CreateSafetyListPredicate() in chrome/browser/actor/execution_engine.cc)
  // evaluates an event that has no distinct source URL: the destination is used
  // as the source. As there, a lack of decision (e.g. because the lists have
  // not been loaded) does not block.
  actor::SafetyListManager::GetInstance()->Find(
      url, url, base::BindOnce([](actor::SafetyListManager::Decision decision) {
                  return decision != actor::SafetyListManager::Decision::kBlock;
                }).Then(std::move(callback)));
}

}  // namespace ttc
