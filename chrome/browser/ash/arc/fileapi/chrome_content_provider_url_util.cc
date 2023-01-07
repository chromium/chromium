// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/chrome_content_provider_url_util.h"

#include <string>

#include "base/strings/escape.h"
#include "base/strings/string_util.h"

namespace arc {

namespace {

constexpr char kChromeContentProviderUrl[] =
    "content://org.chromium.arc.chromecontentprovider/";

}  // namespace

GURL EncodeToChromeContentProviderUrl(const GURL& url) {
  const std::string escaped =
      base::EscapeQueryParamValue(url.spec(), false /* use_plus */);
  return GURL(kChromeContentProviderUrl).Resolve(escaped);
}

GURL DecodeFromChromeContentProviderUrl(
    const GURL& chrome_content_provider_url) {
  const std::string spec = chrome_content_provider_url.spec();
  if (!base::StartsWith(spec, kChromeContentProviderUrl,
                        base::CompareCase::SENSITIVE))
    return GURL();
  const std::string escaped = spec.substr(strlen(kChromeContentProviderUrl));
  return GURL(base::UnescapeBinaryURLComponent(escaped));
}

}  // namespace arc
