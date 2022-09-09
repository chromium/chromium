// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_APP_ID_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_APP_ID_PROVIDER_IMPL_H_

#include "ash/public/cpp/arc_app_id_provider.h"

namespace arc {

class ArcAppIdProviderImpl : public ash::ArcAppIdProvider {
 public:
  ArcAppIdProviderImpl();

  ArcAppIdProviderImpl(const ArcAppIdProviderImpl&) = delete;
  ArcAppIdProviderImpl& operator=(const ArcAppIdProviderImpl&) = delete;

  ~ArcAppIdProviderImpl() override;

  // ash::ArcAppIdProvider:
  std::string GetAppIdByPackageName(const std::string& package_name) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_APP_ID_PROVIDER_IMPL_H_
