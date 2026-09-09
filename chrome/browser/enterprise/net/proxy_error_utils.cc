// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/proxy_error_utils.h"

#include <optional>
#include <utility>

#include "base/values.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_error_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/grit/components_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"

namespace enterprise_net {

content::mojom::AlternativeErrorPageOverrideInfoPtr
GetEnterpriseProxyErrorPageInfo(content::NavigationHandle& navigation_handle,
                                content::BrowserContext* browser_context) {
  if (!IsEnterpriseProxyErrorHandlingEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile) {
    return nullptr;
  }
  auto* error_service =
      EnterpriseProxyErrorServiceFactory::GetForProfile(profile);
  if (!error_service) {
    return nullptr;
  }
  std::optional<EnterpriseProxyErrorData> error_data =
      error_service->TakeDisguisedError(navigation_handle.GetNavigationId());
  if (!error_data) {
    return nullptr;
  }
  base::DictValue params = error_service->GetErrorPageParams(*error_data);
  if (params.empty()) {
    return nullptr;
  }
  auto alternative_error_page_override_info =
      content::mojom::AlternativeErrorPageOverrideInfo::New();
  alternative_error_page_override_info->resource_id =
      IDR_ENTERPRISE_PROXY_ERROR_PAGE_HTML;
  alternative_error_page_override_info->alternative_error_page_params =
      std::move(params);
  return alternative_error_page_override_info;
}

}  // namespace enterprise_net
