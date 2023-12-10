// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_RESOURCE_UTIL_H_
#define ASH_ASSISTANT_UTIL_RESOURCE_UTIL_H_

#include <optional>

#include "base/component_export.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace ash {
namespace assistant {
namespace util {

// Enumeration of resource link types.
enum class ResourceLinkType {
  kUnsupported,
  kIcon,
};

// Enumeration of resource link parameters.
// Examples of usage in comments. Note that actual Assistant resource links are
// prefixed w/ "googleassistant"; "ga" is only used here to avoid line wrapping.
enum class ResourceLinkParam {
  kColor,  // ga://resource?type=icon&name=assistant&&color=AARRGGBB
  kName,   // ga://resource?type=icon&name=assistant
  kType,   // ga://resource?type=icon&name=assistant
};

// Enumeration of icon names.
enum class IconName {
  kAssistant,
  kCalculate,
  kConversionPath,
  kPersonPinCircle,
  kScreenshot,
  kSentimentVerySatisfied,
  kStraighten,
  kTimer,
  kTranslate,
};

// Returns a new resource link, having appended or replaced the color param from
// the original |resource_link| with |color|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL AppendOrReplaceColorParam(const GURL& resource_link, SkColor color);

// Returns a resource link for the specified icon.
COMPONENT_EXPORT(ASSISTANT_UTIL)
GURL CreateIconResourceLink(IconName name,
                            std::optional<SkColor> color = std::nullopt);

// Returns an ImageSkia for the icon resource link specified by |url|. If |url|
// is *not* an icon resource link, the return value is null. If |size| is not
// specified, the vector icon's default size is used.
COMPONENT_EXPORT(ASSISTANT_UTIL)
gfx::ImageSkia CreateVectorIcon(const GURL& url, int size = 0);

// Returns the type of the resource link specified by |url|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
ResourceLinkType GetResourceLinkType(const GURL& url);

// Returns true if the specified |url| is a resource link of the given |type|.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsResourceLinkType(const GURL& url, ResourceLinkType type);

// Returns true if the specified |url| is a resource link, false otherwise.
COMPONENT_EXPORT(ASSISTANT_UTIL) bool IsResourceLinkUrl(const GURL& url);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_RESOURCE_UTIL_H_
