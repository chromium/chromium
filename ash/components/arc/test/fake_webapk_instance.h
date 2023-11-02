// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_WEBAPK_INSTANCE_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_WEBAPK_INSTANCE_H_

#include <unordered_set>

#include "ash/components/arc/mojom/webapk.mojom.h"

namespace arc {

class FakeWebApkInstance : public mojom::WebApkInstance {
 public:
  FakeWebApkInstance();
  FakeWebApkInstance(const FakeWebApkInstance&) = delete;
  FakeWebApkInstance& operator=(const FakeWebApkInstance&) = delete;

  ~FakeWebApkInstance() override;

  // mojom::WebApkInstance overrides:
  void InstallWebApk(const std::string& package_name,
                     uint32_t version,
                     const std::string& app_name,
                     const std::string& token,
                     InstallWebApkCallback callback) override;

  void GetWebApkInfo(const std::string& package_name,
                     GetWebApkInfoCallback callback) override;

  const std::unordered_set<std::string>& handled_packages() {
    return handled_packages_;
  }

  void set_install_result(arc::mojom::WebApkInstallResult result) {
    install_result_ = result;
  }

  void set_web_apk_info(arc::mojom::WebApkInfoPtr web_apk_info) {
    web_apk_info_ = std::move(web_apk_info);
  }

 private:
  std::unordered_set<std::string> handled_packages_;

  arc::mojom::WebApkInstallResult install_result_ =
      arc::mojom::WebApkInstallResult::kSuccess;

  arc::mojom::WebApkInfoPtr web_apk_info_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_WEBAPK_INSTANCE_H_
