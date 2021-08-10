// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_simple_types.h"

namespace crostini {

LinuxPackageInfo::LinuxPackageInfo() = default;
LinuxPackageInfo::LinuxPackageInfo(LinuxPackageInfo&&) = default;
LinuxPackageInfo::LinuxPackageInfo(const LinuxPackageInfo&) = default;
LinuxPackageInfo& LinuxPackageInfo::operator=(LinuxPackageInfo&&) = default;
LinuxPackageInfo& LinuxPackageInfo::operator=(const LinuxPackageInfo&) =
    default;
LinuxPackageInfo::~LinuxPackageInfo() = default;

ContainerInfo::ContainerInfo(std::string container_name,
                             std::string container_username,
                             std::string container_homedir,
                             std::string ipv4_address)
    : name(std::move(container_name)),
      username(std::move(container_username)),
      homedir(std::move(container_homedir)),
      ipv4_address(std::move(ipv4_address)) {}
ContainerInfo::~ContainerInfo() = default;
ContainerInfo::ContainerInfo(ContainerInfo&&) = default;
ContainerInfo::ContainerInfo(const ContainerInfo&) = default;
ContainerInfo& ContainerInfo::operator=(ContainerInfo&&) = default;
ContainerInfo& ContainerInfo::operator=(const ContainerInfo&) = default;
}  // namespace crostini
