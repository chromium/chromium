// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_types.h"

#include "chrome/browser/ash/login/oobe_apps_service/proto/oobe.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"

namespace ash {

OOBEDeviceUseCase::OOBEDeviceUseCase(oobe::proto::OOBEListResponse_Tag tag)
    : id_(std::move(tag.tag())),
      label_(std::move(tag.label())),
      order_(tag.order()),
      image_url_(std::move(tag.image_url())),
      description_(std::move(tag.description())) {}

OOBEDeviceUseCase::OOBEDeviceUseCase(const OOBEDeviceUseCase&) = default;

OOBEDeviceUseCase& OOBEDeviceUseCase::operator=(const OOBEDeviceUseCase&) =
    default;

OOBEDeviceUseCase::~OOBEDeviceUseCase() = default;

const std::string& OOBEDeviceUseCase::GetID() const {
  return id_;
}

const std::string& OOBEDeviceUseCase::GetLabel() const {
  return label_;
}

int OOBEDeviceUseCase::GetOrder() const {
  return order_;
}

const std::string& OOBEDeviceUseCase::GetImageURL() const {
  return image_url_;
}

const std::string& OOBEDeviceUseCase::GetDescription() const {
  return description_;
}

OOBEAppDefinition::OOBEAppDefinition(oobe::proto::OOBEListResponse_App app)
    : app_group_uuid_(std::move(app.app_group_uuid())),
      package_id_(apps::PackageId::FromString(std::move(app.package_id()))),
      name_(std::move(app.name())),
      icon_url_(std::move(app.icon().url())),
      order_(app.order()) {
  for (std::string tag_label : app.tags()) {
    tags_.emplace_back(std::move(tag_label));
  }
}

OOBEAppDefinition::OOBEAppDefinition(const OOBEAppDefinition&) = default;

OOBEAppDefinition& OOBEAppDefinition::operator=(const OOBEAppDefinition&) =
    default;

OOBEAppDefinition::~OOBEAppDefinition() = default;

const std::string& OOBEAppDefinition::GetAppGroupUUID() const {
  return app_group_uuid_;
}

const std::string& OOBEAppDefinition::GetIconURL() const {
  return icon_url_;
}

const std::string& OOBEAppDefinition::GetName() const {
  return name_;
}

int OOBEAppDefinition::GetOrder() const {
  return order_;
}

std::optional<apps::PackageId> OOBEAppDefinition::GetPackageId() const {
  return package_id_;
}

apps::PackageType OOBEAppDefinition::GetPlatform() const {
  if (package_id_.has_value()) {
    return package_id_->package_type();
  }

  return apps::PackageType::kUnknown;
}

const std::vector<std::string>& OOBEAppDefinition::GetTags() const {
  return tags_;
}

}  // namespace ash
