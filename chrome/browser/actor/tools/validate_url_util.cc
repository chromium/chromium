// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/validate_url_util.h"

#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "url/gurl.h"

namespace actor {

namespace {

mojom::ActionResultPtr UrlCheckToActionResult(MayActOnUrlBlockReason reason) {
  return reason == MayActOnUrlBlockReason::kAllowed
             ? MakeOkResult()
             : MakeResult(
                   BlockReasonToResultCode(reason, /*for_navigation=*/false));
}

}  // namespace

void ValidateUrlIsAcceptableNavigationDestination(const GURL& url,
                                                  ToolDelegate& tool_delegate,
                                                  ToolCallback callback) {
  if (!url.is_valid()) {
    // URL is invalid.
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kNavigateInvalidUrl));
    return;
  }

  tool_delegate.IsAcceptableNavigationDestination(
      url, base::BindOnce(&UrlCheckToActionResult).Then(std::move(callback)));
}

}  // namespace actor
