// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/system_info.h"

#include "base/memory/ptr_util.h"

namespace ash {
namespace eche_app {

SystemInfo::Builder::Builder() = default;

SystemInfo::Builder::~Builder() = default;

std::unique_ptr<SystemInfo> SystemInfo::Builder::Build() {
  return base::WrapUnique(new SystemInfo(device_name_, board_name_));
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

SystemInfo::SystemInfo(const SystemInfo& other) = default;

SystemInfo::~SystemInfo() = default;

SystemInfo::SystemInfo(const std::string& device_name,
                       const std::string& board_name)
    : device_name_(device_name), board_name_(board_name) {}

}  // namespace eche_app
}  // namespace ash
