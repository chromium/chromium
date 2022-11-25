// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"

#include "base/no_destructor.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"
#include "chromeos/ash/services/secure_channel/secure_channel_base.h"
#include "chromeos/ash/services/secure_channel/secure_channel_initializer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash {
namespace secure_channel {

SecureChannelClientProvider::SecureChannelClientProvider() = default;

SecureChannelClientProvider::~SecureChannelClientProvider() = default;

// static
SecureChannelClientProvider* SecureChannelClientProvider::GetInstance() {
  static base::NoDestructor<SecureChannelClientProvider> provider;
  return provider.get();
}

SecureChannelClient* SecureChannelClientProvider::GetClient() {
  if (!secure_channel_client_) {
    static base::NoDestructor<std::unique_ptr<SecureChannelBase>> instance{
        [] { return SecureChannelInitializer::Factory::Create(); }()};

    mojo::PendingRemote<mojom::SecureChannel> channel;
    (*instance)->BindReceiver(channel.InitWithNewPipeAndPassReceiver());
    secure_channel_client_ =
        SecureChannelClientImpl::Factory::Create(std::move(channel));
  }

  return secure_channel_client_.get();
}

}  // namespace secure_channel
}  // namespace ash
