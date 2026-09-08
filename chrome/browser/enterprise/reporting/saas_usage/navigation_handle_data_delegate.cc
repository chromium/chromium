// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/navigation_handle_data_delegate.h"

#include <string_view>
#include <utility>

#include "content/public/browser/navigation_handle.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"

namespace enterprise_reporting {

NavigationHandleDataDelegate::NavigationHandleDataDelegate(
    content::NavigationHandle& navigation_handle)
    : navigation_handle_(navigation_handle) {}

void NavigationHandleDataDelegate::GetEncryptionProtocol(
    EncryptionProtocolCallback callback) const {
  const auto& ssl_info = navigation_handle_->GetSSLInfo();
  if (!ssl_info.has_value()) {
    std::move(callback).Run("Unencrypted");
    return;
  }
  net::SSLVersion ssl_version =
      net::SSLConnectionStatusToVersion(ssl_info->connection_status);
  if (ssl_version == net::SSL_CONNECTION_VERSION_UNKNOWN) {
    std::move(callback).Run("Unknown");
    return;
  }
  const char* encryption_protocol = "";
  net::SSLVersionToString(&encryption_protocol, ssl_version);
  std::move(callback).Run(encryption_protocol);
}

GURL NavigationHandleDataDelegate::GetUrl() const {
  return navigation_handle_->GetURL();
}

}  // namespace enterprise_reporting
