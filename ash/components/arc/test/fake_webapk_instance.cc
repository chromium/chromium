// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/fake_webapk_instance.h"

namespace arc {

FakeWebApkInstance::FakeWebApkInstance() = default;

FakeWebApkInstance::~FakeWebApkInstance() = default;

void FakeWebApkInstance::InstallWebApk(const std::string& package_name,
                                       uint32_t version,
                                       const std::string& app_name,
                                       const std::string& token,
                                       InstallWebApkCallback callback) {
  handled_packages_.insert(package_name);
  std::move(callback).Run(install_result_);
}

void FakeWebApkInstance::GetWebApkInfo(const std::string& package_name,
                                       GetWebApkInfoCallback callback) {
  std::move(callback).Run(std::move(web_apk_info_));
}

}  // namespace arc
