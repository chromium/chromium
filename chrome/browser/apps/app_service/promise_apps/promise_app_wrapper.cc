// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"

#include <optional>
#include <vector>

#include "base/strings/string_util.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "url/gurl.h"

namespace apps {

IconWrapper::IconWrapper(proto::PromiseAppResponse::Icon icon_proto)
    : icon_proto_(icon_proto) {}

GURL IconWrapper::GetUrl() const {
  return GURL(icon_proto_.url());
}

std::optional<int> IconWrapper::GetWidthInPixels() const {
  std::optional<int> width;
  if (icon_proto_.has_width_in_pixels()) {
    width = icon_proto_.width_in_pixels();
  }
  return width;
}

std::string IconWrapper::GetMimeType() const {
  return icon_proto_.mime_type();
}

bool IconWrapper::IsMaskingAllowed() const {
  return icon_proto_.is_masking_allowed();
}

PromiseAppWrapper::PromiseAppWrapper(
    proto::PromiseAppResponse promise_app_proto)
    : promise_app_proto_(promise_app_proto),
      package_id_(PackageId::FromString(promise_app_proto.package_id())) {}

PromiseAppWrapper::PromiseAppWrapper(const PromiseAppWrapper&) = default;
PromiseAppWrapper& PromiseAppWrapper::operator=(const PromiseAppWrapper&) =
    default;
PromiseAppWrapper::~PromiseAppWrapper() = default;

std::optional<PackageId> PromiseAppWrapper::GetPackageId() const {
  return package_id_;
}

std::optional<std::string> PromiseAppWrapper::GetName() const {
  std::optional<std::string> name;
  if (promise_app_proto_.has_name()) {
    name = promise_app_proto_.name();
  }
  return name;
}

std::vector<IconWrapper> PromiseAppWrapper::GetIcons() const {
  std::vector<IconWrapper> icon_data;
  icon_data.reserve(promise_app_proto_.icons().size());
  for (auto icon : promise_app_proto_.icons()) {
    icon_data.emplace_back(icon);
  }
  return icon_data;
}

std::ostream& operator<<(std::ostream& os, const IconWrapper& icon) {
  os << std::boolalpha;
  os << "* Url: " << icon.GetUrl().spec() << std::endl;
  os << "* Width in pixels: "
     << (icon.GetWidthInPixels().has_value()
             ? base::NumberToString(icon.GetWidthInPixels().value())
             : "N/A")
     << std::endl;
  os << "* Mime type: " << icon.GetMimeType() << std::endl;
  os << "* Is masking allowed: " << icon.IsMaskingAllowed() << std::endl;
  os << std::noboolalpha;
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const PromiseAppWrapper& promise_app) {
  os << std::boolalpha;
  os << "- Package Id: "
     << (promise_app.GetPackageId().has_value()
             ? promise_app.GetPackageId()->ToString()
             : "N/A")
     << std::endl;
  os << "- Name: " << promise_app.GetName().value_or("N/A") << std::endl;
  for (const IconWrapper& icon : promise_app.GetIcons()) {
    os << "- Icons: " << icon << std::endl;
  }
  os << std::noboolalpha;
  return os;
}

}  // namespace apps
