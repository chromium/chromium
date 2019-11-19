// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ARC_APP_ID_PROVIDER_H_
#define ASH_PUBLIC_CPP_ARC_APP_ID_PROVIDER_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

class ASH_PUBLIC_EXPORT ArcAppIdProvider {
 public:
  static ArcAppIdProvider* Get();

  virtual std::string GetAppIdByPackageName(
      const std::string& package_name) = 0;

 protected:
  ArcAppIdProvider();
  virtual ~ArcAppIdProvider();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ARC_APP_ID_PROVIDER_H_
