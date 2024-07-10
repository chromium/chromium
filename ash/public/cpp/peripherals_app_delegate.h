// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PERIPHERALS_APP_DELEGATE_H_
#define ASH_PUBLIC_CPP_PERIPHERALS_APP_DELEGATE_H_

#include <optional>
#include <string>

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/functional/callback_forward.h"

namespace ash {

class PeripheralsAppDelegate {
 public:
  PeripheralsAppDelegate() = default;
  PeripheralsAppDelegate(const PeripheralsAppDelegate&) = delete;
  PeripheralsAppDelegate& operator=(const PeripheralsAppDelegate&) = delete;
  virtual ~PeripheralsAppDelegate() = default;
  virtual void GetCompanionAppInfo(
      const std::string& device_key,
      base::OnceCallback<void(const std::optional<mojom::CompanionAppInfo>&)>
          callback) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_PERIPHERALS_APP_DELEGATE_H_
