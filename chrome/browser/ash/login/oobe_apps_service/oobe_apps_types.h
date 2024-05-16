// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_TYPES_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ash/login/oobe_apps_service/proto/oobe.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"

namespace ash {

// A wrapper struct for the OOBE Tag proto.
class OOBEDeviceUseCase {
 public:
  explicit OOBEDeviceUseCase(oobe::proto::OOBEListResponse_Tag);
  OOBEDeviceUseCase(const OOBEDeviceUseCase&);
  OOBEDeviceUseCase& operator=(const OOBEDeviceUseCase&);
  ~OOBEDeviceUseCase();

  const std::string& GetID() const;

  const std::string& GetLabel() const;

  int GetOrder() const;

  const std::string& GetImageURL() const;

  const std::string& GetDescription() const;

  // Overloading < operator for sorting.
  bool operator<(const OOBEDeviceUseCase& obj) const {
    return order_ < obj.order_;
  }

 private:
  std::string id_;

  std::string label_;

  int order_;

  std::string image_url_;

  std::string description_;
};

// A wrapper class around an OOBE App proto to allow for easier
// extraction and conversion of information.
class OOBEAppDefinition {
 public:
  explicit OOBEAppDefinition(oobe::proto::OOBEListResponse_App);
  OOBEAppDefinition(const OOBEAppDefinition&);
  OOBEAppDefinition& operator=(const OOBEAppDefinition&);
  ~OOBEAppDefinition();

  const std::string& GetAppGroupUUID() const;

  const std::string& GetIconURL() const;

  const std::string& GetName() const;

  int GetOrder() const;

  std::optional<apps::PackageId> GetPackageId() const;

  apps::PackageType GetPlatform() const;

  const std::vector<std::string>& GetTags() const;

 private:
  std::string app_group_uuid_;

  // This class automatically parses corresponding package types from the
  // given string. If we got a malformed response (package type doesn't
  // correspond to any supported app type) std::nullopt will be returned.
  std::optional<apps::PackageId> package_id_;

  std::string name_;

  // Not storing other data about the icon as we will load it from the WebView
  // and only URL is needed in that case.
  std::string icon_url_;

  // List of device use-case ids to which this app belongs.
  std::vector<std::string> tags_;

  int order_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_APPS_SERVICE_OOBE_APPS_TYPES_H_
