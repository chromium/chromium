// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_
#define ASH_COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_

#include <utility>

#include "ash/components/arc/session/connection_holder.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

// Thin interface to wrap Remote<T> with type erasure.
class MojoChannelBase {
 public:
  MojoChannelBase(const MojoChannelBase&) = delete;
  MojoChannelBase& operator=(const MojoChannelBase&) = delete;

  virtual ~MojoChannelBase() = default;

 protected:
  MojoChannelBase() = default;
};

// Thin wrapper for Remote<T>, where T is one of ARC mojo Instance class.
template <typename InstanceType, typename HostType>
class MojoChannel : public MojoChannelBase {
 public:
  MojoChannel(ConnectionHolder<InstanceType, HostType>* holder,
              mojo::PendingRemote<InstanceType> remote)
      : holder_(holder), remote_(std::move(remote)) {
    // Delay registration to the ConnectionHolder until the version is ready.
  }

  MojoChannel(const MojoChannel&) = delete;
  MojoChannel& operator=(const MojoChannel&) = delete;

  ~MojoChannel() override { holder_->CloseInstance(remote_.get()); }

  void set_disconnect_handler(base::OnceClosure error_handler) {
    remote_.set_disconnect_handler(std::move(error_handler));
  }

  void QueryVersion() {
    // Note: the callback will not be called if `remote_` is destroyed.
    remote_.QueryVersion(
        base::BindOnce(&MojoChannel::OnVersionReady, base::Unretained(this)));
  }

  // `OnVersionReady` is called directly when the interface version is already
  // stored in `remote_`.
  void OnVersionReady(uint32_t unused_version) {
    holder_->SetInstance(remote_.get(), remote_.version());
  }

 private:
  // Externally owned ConnectionHolder instance.
  const raw_ptr<ConnectionHolder<InstanceType, HostType>> holder_;

  // Put as a last member to ensure that any callback tied to the `remote_`
  // is not invoked.
  mojo::Remote<InstanceType> remote_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_
