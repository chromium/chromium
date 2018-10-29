// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
#define ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_

#include <map>
#include <string>

#include "ash/ash_export.h"
#include "base/optional.h"

class GURL;

namespace ash {
namespace assistant {
namespace util {

// Enumeration of deep link types.
enum class DeepLinkType {
  kUnsupported,
  kChromeSettings,
  kFeedback,
  kOnboarding,
  kQuery,
  kReminders,
  kScreenshot,
  kSettings,
  kTaskManager,
  kWhatsOnMyScreen,
};

// Enumeration of deep link parameters.
enum class DeepLinkParam {
  kPage,
  kQuery,
  kRelaunch,
};

// Returns a deep link to top level Assistant Settings.
ASH_EXPORT GURL CreateAssistantSettingsDeepLink();

// Returns a deep link to initiate a screen context interaction.
ASH_EXPORT GURL CreateWhatsOnMyScreenDeepLink();

// Returns the parsed parameters for the specified |deep_link|. If the supplied
// argument is not a supported deep link or if no parameters are found, an empty
// map is returned.
ASH_EXPORT std::map<std::string, std::string> GetDeepLinkParams(
    const GURL& deep_link);

// Returns a specific string |param| from the given parameters. If the desired
// parameter is not found, and empty value is returned.
ASH_EXPORT base::Optional<std::string> GetDeepLinkParam(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns a specific bool |param| from the given parameters. If the desired
// parameter is not found or is not a bool, an empty value is returned.
ASH_EXPORT base::Optional<bool> GetDeepLinkParamAsBool(
    const std::map<std::string, std::string>& params,
    DeepLinkParam param);

// Returns the deep link type of the specified |url|. If the specified url is
// not a supported deep link, DeepLinkType::kUnsupported is returned.
ASH_EXPORT DeepLinkType GetDeepLinkType(const GURL& url);

// Returns true if the specified |url| is a deep link of the given |type|.
ASH_EXPORT bool IsDeepLinkType(const GURL& url, DeepLinkType type);

// Returns true if the specified |url| is a deep link, false otherwise.
ASH_EXPORT bool IsDeepLinkUrl(const GURL& url);

// Returns the URL for the specified Chrome Settings |page|. If page is absent
// or not allowed, the URL will be for top-level Chrome Settings.
ASH_EXPORT GURL GetChromeSettingsUrl(const base::Optional<std::string>& page);

// Returns the web URL for the specified |deep_link|. A return value will only
// be present if |deep_link| is a web deep link as identified by the
// IsWebDeepLink(GURL) API.
ASH_EXPORT base::Optional<GURL> GetWebUrl(const GURL& deep_link);

// Returns the web URL for a deep link of the specified |type|. A return value
// will only be present if the deep link type is a web deep link type as
// identified by the IsWebDeepLinkType(DeepLinkType) API.
ASH_EXPORT base::Optional<GURL> GetWebUrl(DeepLinkType type);

// Returns true if the specified |deep_link| is a web deep link.
ASH_EXPORT bool IsWebDeepLink(const GURL& deep_link);

// Returns true if the specified deep link |type| is a web deep link.
ASH_EXPORT bool IsWebDeepLinkType(DeepLinkType type);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_DEEP_LINK_UTIL_H_
