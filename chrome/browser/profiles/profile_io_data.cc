// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_io_data.h"

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "net/net_buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/webapps/isolated_web_apps/scheme.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

// static
bool ProfileIOData::IsHandledProtocol(std::string_view scheme) {
  DCHECK_EQ(scheme, base::ToLowerASCII(scheme));

  constexpr auto kProtocolList = base::MakeFixedFlatSet<std::string_view>({
      url::kHttpScheme,
      url::kHttpsScheme,
#if BUILDFLAG(ENABLE_WEBSOCKETS)
      url::kWsScheme,
      url::kWssScheme,
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
      url::kFileScheme,
      content::kChromeDevToolsScheme,
      dom_distiller::kDomDistillerScheme,
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      extensions::kExtensionScheme,
#endif
      content::kChromeUIScheme,
      content::kChromeUIUntrustedScheme,
      url::kDataScheme,
#if BUILDFLAG(IS_CHROMEOS)
      content::kExternalFileScheme,
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_ANDROID)
      url::kContentScheme,
#endif  // BUILDFLAG(IS_ANDROID)
      url::kAboutScheme,
      url::kBlobScheme,
      url::kFileSystemScheme,
      chrome::kChromeSearchScheme,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
      webapps::kIsolatedAppScheme,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
  });

  return kProtocolList.contains(scheme);
}

// static
bool ProfileIOData::IsHandledURL(const GURL& url) {
  if (!url.is_valid()) {
    // We handle error cases.
    return true;
  }

  return IsHandledProtocol(url.GetScheme());
}
