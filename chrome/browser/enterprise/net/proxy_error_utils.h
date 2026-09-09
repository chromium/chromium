// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_PROXY_ERROR_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_NET_PROXY_ERROR_UTILS_H_

#include "content/public/common/alternative_error_page_override_info.mojom.h"

namespace content {
class BrowserContext;
class NavigationHandle;
}  // namespace content

namespace enterprise_net {

// Populates alternative error page override info for disguised enterprise proxy
// errors if error handling is enabled and a disguised error was recorded for
// the given navigation. Returns nullptr if no enterprise proxy error applies.
content::mojom::AlternativeErrorPageOverrideInfoPtr
GetEnterpriseProxyErrorPageInfo(content::NavigationHandle& navigation_handle,
                                content::BrowserContext* browser_context);

}  // namespace enterprise_net

#endif  // CHROME_BROWSER_ENTERPRISE_NET_PROXY_ERROR_UTILS_H_
