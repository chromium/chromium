// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_
#define CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace secure_channel {

class SecureChannelClient;

// Singleton that owns a single SecureChannelClient instance associated with the
// browser process.
class SecureChannelClientProvider {
 public:
  static SecureChannelClientProvider* GetInstance();

  SecureChannelClient* GetClient();

 private:
  friend class base::NoDestructor<SecureChannelClientProvider>;

  SecureChannelClientProvider();
  virtual ~SecureChannelClientProvider();

  std::unique_ptr<SecureChannelClient> secure_channel_client_;

  DISALLOW_COPY_AND_ASSIGN(SecureChannelClientProvider);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_
