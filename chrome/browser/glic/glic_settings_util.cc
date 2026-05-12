// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_settings_util.h"

#include <string>
#include <string_view>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace glic {

GURL GetHelpCenterUrl(std::string_view url_string) {
  GURL url(url_string);
  std::string p_val;
  if (!net::GetValueForKeyInQuery(url, "p", &p_val)) {
    return url;
  }
  std::string_view platform_suffix = GetPlatformHelpSuffix();
  if (!platform_suffix.empty() && !base::EndsWith(p_val, platform_suffix)) {
    return net::AppendOrReplaceQueryParameter(
        url, "p", base::StrCat({p_val, platform_suffix}));
  }
  return url;
}

}  // namespace glic
