// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_

#include <memory>

// TODO(https://crbug.com/1164001): move to forward declaration.
#include "ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "base/no_destructor.h"

namespace ash {
namespace secure_channel {

// Singleton that owns a single SecureChannelClient instance associated with the
// browser process.
class SecureChannelClientProvider {
 public:
  static SecureChannelClientProvider* GetInstance();

  SecureChannelClientProvider(const SecureChannelClientProvider&) = delete;
  SecureChannelClientProvider& operator=(const SecureChannelClientProvider&) =
      delete;

  SecureChannelClient* GetClient();

 private:
  friend class base::NoDestructor<SecureChannelClientProvider>;

  SecureChannelClientProvider();
  virtual ~SecureChannelClientProvider();

  std::unique_ptr<SecureChannelClient> secure_channel_client_;
};

}  // namespace secure_channel
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace secure_channel {
using ::ash::secure_channel::SecureChannelClientProvider;
}
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_
