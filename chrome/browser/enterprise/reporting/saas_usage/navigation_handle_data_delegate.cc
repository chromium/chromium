// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/navigation_handle_data_delegate.h"

#include <optional>
#include <string_view>
#include <utility>

#include "components/enterprise/browser/reporting/saas_usage/saas_usage_aggregation_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"

namespace enterprise_reporting {

NavigationHandleDataDelegate::NavigationHandleDataDelegate(
    content::NavigationHandle& navigation_handle)
    : navigation_handle_(navigation_handle) {}

void NavigationHandleDataDelegate::GetEncryptionProtocol(
    EncryptionProtocolCallback callback) const {
  const auto& ssl_info = navigation_handle_->GetSSLInfo();
  std::optional<net::SSLVersion> ssl_version;
  if (ssl_info.has_value()) {
    ssl_version =
        net::SSLConnectionStatusToVersion(ssl_info->connection_status);
  }
  std::move(callback).Run(GetEncryptionProtocolString(ssl_version));
}

GURL NavigationHandleDataDelegate::GetUrl() const {
  return navigation_handle_->GetURL();
}

}  // namespace enterprise_reporting
