// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/content_area_user_provider.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/connectors/core/content_area_user_provider.h"

namespace enterprise_data_protection {

std::string GetActiveContentAreaUser(
    const content::ClipboardEndpoint& endpoint) {
  if (!endpoint.data_transfer_endpoint() ||
      !endpoint.data_transfer_endpoint()->IsUrlType() ||
      !endpoint.data_transfer_endpoint()->GetURL() ||
      !endpoint.browser_context()) {
    return "";
  }

  auto* im = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(endpoint.browser_context()));
  if (!im) {
    return "";
  }

  return enterprise_connectors::GetActiveContentAreaUser(
      im, *endpoint.data_transfer_endpoint()->GetURL());
}

}  // namespace enterprise_data_protection
