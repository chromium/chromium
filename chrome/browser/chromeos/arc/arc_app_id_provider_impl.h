// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_APP_ID_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_APP_ID_PROVIDER_IMPL_H_

#include "ash/public/cpp/arc_app_id_provider.h"
#include "base/macros.h"

namespace arc {

class ArcAppIdProviderImpl : public ash::ArcAppIdProvider {
 public:
  ArcAppIdProviderImpl();
  ~ArcAppIdProviderImpl() override;

  // ash::ArcAppIdProvider:
  std::string GetAppIdByPackageName(const std::string& package_name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppIdProviderImpl);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_APP_ID_PROVIDER_IMPL_H_
