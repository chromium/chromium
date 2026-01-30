// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/navigation_handle_data_delegate.h"

#include <string>

#include "content/public/browser/navigation_handle.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"

namespace enterprise_reporting {

NavigationHandleDataDelegate::NavigationHandleDataDelegate(
    content::NavigationHandle& navigation_handle)
    : navigation_handle_(navigation_handle) {}

std::string NavigationHandleDataDelegate::GetEncryptionProtocol() const {
  const auto& ssl_info = navigation_handle_->GetSSLInfo();
  if (!ssl_info.has_value()) {
    return "Unencrypted";
  }
  const char* encryption_protocol = "";
  net::SSLVersion ssl_version =
      net::SSLConnectionStatusToVersion(ssl_info->connection_status);
  net::SSLVersionToString(&encryption_protocol, ssl_version);
  return encryption_protocol;
}

GURL NavigationHandleDataDelegate::GetUrl() const {
  return navigation_handle_->GetURL();
}

}  // namespace enterprise_reporting
