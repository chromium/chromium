// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_VALIDATE_URL_UTIL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_VALIDATE_URL_UTIL_H_

#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor.mojom-forward.h"

class GURL;

namespace actor {

class ToolDelegate;

// Validates whether the URL is valid and acceptable to navigate to, invoking
// the callback with the result.
void ValidateUrlIsAcceptableNavigationDestination(const GURL& url,
                                                  ToolDelegate& tool_delegate,
                                                  ToolCallback callback);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_VALIDATE_URL_UTIL_H_
