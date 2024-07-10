// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_DEVICE_SETTINGS_PERIPHERALS_APP_DELEGATE_IMPL_H_
#define CHROME_BROWSER_ASH_INPUT_DEVICE_SETTINGS_PERIPHERALS_APP_DELEGATE_IMPL_H_

#include "ash/public/cpp/peripherals_app_delegate.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/functional/callback.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class PeripheralsAppDelegateImpl : public PeripheralsAppDelegate {
 public:
  PeripheralsAppDelegateImpl();
  PeripheralsAppDelegateImpl(const PeripheralsAppDelegateImpl&) = delete;
  PeripheralsAppDelegateImpl& operator=(const PeripheralsAppDelegateImpl&) =
      delete;
  ~PeripheralsAppDelegateImpl() override;

  // PeripheralsAppDelegate:
  void GetCompanionAppInfo(
      const std::string& device_key,
      base::OnceCallback<void(const std::optional<mojom::CompanionAppInfo>&)>
          callback) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_DEVICE_SETTINGS_PERIPHERALS_APP_DELEGATE_IMPL_H_
