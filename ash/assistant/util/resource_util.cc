// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/resource_util.h"

#include <map>
#include <sstream>
#include <string>
#include <string_view>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "net/base/url_util.h"

namespace ash {
namespace assistant {
namespace util {

namespace {

// Constants -------------------------------------------------------------------

// Prefix for all resource links.
constexpr char kResourceLinkPrefix[] = "googleassistant://resource";

// Param keys.
constexpr char kColorParamKey[] = "color";
constexpr char kNameParamKey[] = "name";
constexpr char kTypeParamKey[] = "type";

// Resource types.
constexpr char kIconResourceType[] = "icon";

// Icon names.
constexpr char kAssistantIconName[] = "assistant";
constexpr char kCalculateIconName[] = "calculate";
constexpr char kConversionPathIconName[] = "conversationPath";
constexpr char kPersonPinCircleIconName[] = "personPinCircle";
constexpr char kScreenshotIconName[] = "screenshot";
constexpr char kSentimentVerySatisfiedIconName[] = "sentimentVerySatisfied";
constexpr char kStraightenIconName[] = "straighten";
constexpr char kTimerIconName[] = "timer";
constexpr char kTranslateIconName[] = "translate";

// Helpers ---------------------------------------------------------------------

SkColor ToColor(const std::string& aarrggbb) {
  SkColor color = gfx::kPlaceholderColor;
  std::istringstream hex_stream(aarrggbb);
  hex_stream >> std::hex >> color;
  return color;
}

IconName ToIconName(const std::string& name) {
  const std::map<std::string, IconName> icon_names_by_string_name = {
      {kAssistantIconName, IconName::kAssistant},
      {kCalculateIconName, IconName::kCalculate},
      {kConversionPathIconName, IconName::kConversionPath},
      {kPersonPinCircleIconName, IconName::kPersonPinCircle},
      {kScreenshotIconName, IconName::kScreenshot},
      {kSentimentVerySatisfiedIconName, IconName::kSentimentVerySatisfied},
      {kStraightenIconName, IconName::kStraighten},
      {kTimerIconName, IconName::kTimer},
      {kTranslateIconName, IconName::kTranslate},
  };
  auto it = icon_names_by_string_name.find(name);
  DCHECK(it != icon_names_by_string_name.end());
  return it->second;
}

std::string ToString(SkColor color) {
  std::stringstream hex_stream;
  hex_stream << std::hex << color;
  return hex_stream.str();
}

std::string ToString(IconName name) {
  switch (name) {
    case IconName::kAssistant:
      return kAssistantIconName;
    case IconName::kCalculate:
      return kCalculateIconName;
    case IconName::kConversionPath:
      return kConversionPathIconName;
    case IconName::kPersonPinCircle:
      return kPersonPinCircleIconName;
    case IconName::kScreenshot:
      return kScreenshotIconName;
    case IconName::kSentimentVerySatisfied:
      return kSentimentVerySatisfiedIconName;
    case IconName::kStraighten:
      return kStraightenIconName;
    case IconName::kTimer:
      return kTimerIconName;
    case IconName::kTranslate:
      return kTranslateIconName;
  }
  NOTREACHED();
}

std::string ToString(ResourceLinkParam param) {
  switch (param) {
    case ResourceLinkParam::kColor:
      return kColorParamKey;
    case ResourceLinkParam::kName:
      return kNameParamKey;
    case ResourceLinkParam::kType:
      return kTypeParamKey;
  }
  NOTREACHED();
}

std::string ToString(ResourceLinkType type) {
  switch (type) {
    case ResourceLinkType::kIcon:
      return kIconResourceType;
    case ResourceLinkType::kUnsupported:
      return std::string();
  }
  NOTREACHED();
}

ResourceLinkType ToType(const std::string& type) {
  if (type == kIconResourceType)
    return ResourceLinkType::kIcon;
  return ResourceLinkType::kUnsupported;
}

const gfx::VectorIcon& ToVectorIcon(IconName name) {
  switch (name) {
    case IconName::kAssistant:
      return chromeos::kAssistantIcon;
    case IconName::kCalculate:
      return chromeos::kCalculateIcon;
    case IconName::kConversionPath:
      return chromeos::kConversionPathIcon;
    case IconName::kPersonPinCircle:
      return chromeos::kPersonPinCircleIcon;
    case IconName::kScreenshot:
      return chromeos::kScreenshotIcon;
    case IconName::kSentimentVerySatisfied:
      return chromeos::kSentimentVerySatisfiedIcon;
    case IconName::kStraighten:
      return chromeos::kStraightenIcon;
    case IconName::kTimer:
      return chromeos::kTimerIcon;
    case IconName::kTranslate:
      return chromeos::kTranslateIcon;
  }
  NOTREACHED();
}

const gfx::VectorIcon& ToVectorIcon(const std::string& name) {
  return ToVectorIcon(ToIconName(name));
}

std::optional<std::string> GetParam(const GURL& url, ResourceLinkParam param) {
  if (!url.has_query())
    return std::nullopt;

  const std::string param_key = ToString(param);
  const std::string_view query_piece = url.query_piece();
  url::Component query(0, query_piece.length()), key, value;
  while (url::ExtractQueryKeyValue(query_piece, &query, &key, &value)) {
    if (query_piece.substr(key.begin, base::checked_cast<size_t>(key.len)) ==
        param_key) {
      return std::string(query_piece.substr(
          value.begin, base::checked_cast<size_t>(value.len)));
    }
  }

  return std::nullopt;
}

}  // namespace

// Utilities -------------------------------------------------------------------

GURL AppendOrReplaceColorParam(const GURL& resource_link, SkColor color) {
  DCHECK(IsResourceLinkType(resource_link, ResourceLinkType::kIcon));
  return net::AppendOrReplaceQueryParameter(
      resource_link, ToString(ResourceLinkParam::kColor), ToString(color));
}

GURL CreateIconResourceLink(IconName name, std::optional<SkColor> color) {
  GURL icon_resource_link(kResourceLinkPrefix);
  icon_resource_link = net::AppendOrReplaceQueryParameter(
      icon_resource_link, ToString(ResourceLinkParam::kType),
      ToString(ResourceLinkType::kIcon));
  icon_resource_link = net::AppendOrReplaceQueryParameter(
      icon_resource_link, ToString(ResourceLinkParam::kName), ToString(name));
  if (color.has_value()) {
    icon_resource_link = net::AppendOrReplaceQueryParameter(
        icon_resource_link, ToString(ResourceLinkParam::kColor),
        ToString(color.value()));
  }
  return icon_resource_link;
}

gfx::ImageSkia CreateVectorIcon(const GURL& url, int size) {
  if (!IsResourceLinkType(url, ResourceLinkType::kIcon))
    return gfx::ImageSkia();

  const gfx::VectorIcon& icon =
      ToVectorIcon((GetParam(url, ResourceLinkParam::kName).value_or("")));

  SkColor color = ToColor(GetParam(url, ResourceLinkParam::kColor)
                              .value_or(ToString(gfx::kPlaceholderColor)));

  return gfx::CreateVectorIcon(gfx::IconDescription(icon, size, color));
}

ResourceLinkType GetResourceLinkType(const GURL& url) {
  return IsResourceLinkUrl(url)
             ? ToType(GetParam(url, ResourceLinkParam::kType).value_or(""))
             : ResourceLinkType::kUnsupported;
}

bool IsResourceLinkType(const GURL& url, ResourceLinkType type) {
  return GetResourceLinkType(url) == type;
}

bool IsResourceLinkUrl(const GURL& url) {
  return base::StartsWith(url.spec(), kResourceLinkPrefix,
                          base::CompareCase::SENSITIVE);
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
