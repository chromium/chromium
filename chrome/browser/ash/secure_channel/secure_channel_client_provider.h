// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_
#define CHROME_BROWSER_ASH_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_

#include <memory>

#include "base/no_destructor.h"

namespace ash {
namespace secure_channel {

class SecureChannelClient;

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

#endif  // CHROME_BROWSER_ASH_SECURE_CHANNEL_SECURE_CHANNEL_CLIENT_PROVIDER_H_
