// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_EXTENSION_INFO_PRIVATE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_EXTENSION_INFO_PRIVATE_ASH_H_

#include <string>
#include <vector>

#include "chromeos/crosapi/mojom/extension_info_private.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// The ash-chrome implementation of the ExtensionInfoPrivate crosapi interface.
// This class must only be used from the main thread.
class ExtensionInfoPrivateAsh : public mojom::ExtensionInfoPrivate {
 public:
  ExtensionInfoPrivateAsh();
  ExtensionInfoPrivateAsh(const ExtensionInfoPrivateAsh&) = delete;
  ExtensionInfoPrivateAsh& operator=(const ExtensionInfoPrivateAsh&) = delete;
  ~ExtensionInfoPrivateAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::ExtensionInfoPrivate> receiver);

  // mojom::ExtensionInfoPrivate:
  void GetSystemProperties(const std::vector<std::string>& property_names,
                           GetSystemPropertiesCallback callback) override;
  void SetTimezone(const std::string& value) override;
  void SetBool(const std::string& property_name,
               bool value,
               SetBoolCallback callback) override;
  void IsTabletModeEnabled(IsTabletModeEnabledCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::ExtensionInfoPrivate> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_EXTENSION_INFO_PRIVATE_ASH_H_
