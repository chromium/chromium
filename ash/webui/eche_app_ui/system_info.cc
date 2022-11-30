// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/system_info.h"

#include "base/memory/ptr_util.h"

namespace ash {
namespace eche_app {

SystemInfo::Builder::Builder() = default;

SystemInfo::Builder::~Builder() = default;

std::unique_ptr<SystemInfo> SystemInfo::Builder::Build() {
  return base::WrapUnique(
      new SystemInfo(device_name_, board_name_, gaia_id_, device_type_));
}

SystemInfo::Builder& SystemInfo::Builder::SetDeviceName(
    const std::string& device_name) {
  device_name_ = device_name;
  return *this;
}

SystemInfo::Builder& SystemInfo::Builder::SetBoardName(
    const std::string& board_name) {
  board_name_ = board_name;
  return *this;
}

SystemInfo::Builder& SystemInfo::Builder::SetGaiaId(
    const std::string& gaia_id) {
  gaia_id_ = gaia_id;
  return *this;
}

SystemInfo::Builder& SystemInfo::Builder::SetDeviceType(
    const std::string& device_type) {
  device_type_ = device_type;
  return *this;
}

SystemInfo::SystemInfo(const SystemInfo& other) = default;

SystemInfo::~SystemInfo() = default;

SystemInfo::SystemInfo(const std::string& device_name,
                       const std::string& board_name,
                       const std::string& gaia_id,
                       const std::string& device_type)
    : device_name_(device_name),
      board_name_(board_name),
      gaia_id_(gaia_id),
      device_type_(device_type) {}

}  // namespace eche_app
}  // namespace ash
