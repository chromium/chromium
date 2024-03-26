// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OOBE_MOJO_BINDER_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OOBE_MOJO_BINDER_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

template <typename... T>
class OobeMojoBinder;

// Base class for creating mojo binding to a receiver.
// It can be used by all OOBE screens that need to support
// communication from browser to renderer.
template <typename Receiver>
class OobeMojoBinder<Receiver> {
 public:
  explicit OobeMojoBinder(Receiver* impl) : receiver_(impl) {}

  void BindReceiver(mojo::PendingReceiver<Receiver> receiver) {
    CHECK(!receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
  }

 private:
  mojo::Receiver<Receiver> receiver_;
};

// Base class for creating mojo binding to a receiver and
// a remote.
// It can be used by all OOBE screens that need to support
// communication from browser to render, and from renderer
// to browser
template <typename Receiver, typename Remote>
class OobeMojoBinder<Receiver, Remote> {
 public:
  explicit OobeMojoBinder(Receiver* impl) : receiver_(impl) {}

  void BindRemoteAndReceiver(mojo::PendingRemote<Remote> remote,
                             mojo::PendingReceiver<Receiver> receiver) {
    CHECK(!receiver_.is_bound());
    CHECK(!remote_.is_bound());
    receiver_.Bind(std::move(receiver));
    remote_.Bind(std::move(remote));
  }

  mojo::Remote<Remote>* GetRemote() { return &remote_; }

 private:
  mojo::Remote<Remote> remote_;
  mojo::Receiver<Receiver> receiver_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OOBE_MOJO_BINDER_H_
