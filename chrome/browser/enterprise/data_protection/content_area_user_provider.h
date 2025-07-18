// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_CONTENT_AREA_USER_PROVIDER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_CONTENT_AREA_USER_PROVIDER_H_

#include "content/public/browser/clipboard_types.h"

namespace enterprise_data_protection {

// Returns the email of the active Gaia user based of the context of the
// provided clipboard endpoind. Returns an empty string if the endpoint doesn't
// represent a URL or if the endpoint isn't a Workspace site.
std::string GetActiveContentAreaUser(
    const content::ClipboardEndpoint& endpoint);

}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_CONTENT_AREA_USER_PROVIDER_H_
