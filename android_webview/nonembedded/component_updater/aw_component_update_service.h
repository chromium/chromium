// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATE_SERVICE_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATE_SERVICE_H_

#include <memory>
#include <string>

#include "base/no_destructor.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace android_webview {

class MockAwComponentUpdateService;

// Native-side implementation of the AwComponentUpdateService, it initializes
// ComponentUpdateService and registers components.
class AwComponentUpdateService {
 public:
  static AwComponentUpdateService* GetInstance();
  void MaybeStartComponentUpdateService();

  virtual bool NotifyNewVersion(const std::string& component_id,
                                const base::FilePath& install_dir,
                                const base::Version& version);

 private:
  friend base::NoDestructor<AwComponentUpdateService>;
  friend MockAwComponentUpdateService;

  AwComponentUpdateService();
  virtual ~AwComponentUpdateService();

  std::unique_ptr<component_updater::ComponentUpdateService> cus_;
};

void SetAwComponentUpdateServiceForTesting(AwComponentUpdateService* service);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_COMPONENT_UPDATER_AW_COMPONENT_UPDATE_SERVICE_H_