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
template <typename PageHandler>
class OobeMojoBinder<PageHandler> {
 public:
  explicit OobeMojoBinder(PageHandler* impl) : page_handler_receiver_(impl) {}

  void BindPageHandlerReceiver(
      mojo::PendingReceiver<PageHandler> pending_receiver) {
    CHECK(!page_handler_receiver_.is_bound());
    page_handler_receiver_.Bind(std::move(pending_receiver));
  }

 private:
  mojo::Receiver<PageHandler> page_handler_receiver_;
};

// Base class for creating mojo binding to a receiver and
// a remote.
// It can be used by all OOBE screens that need to support
// communication from browser to render, and from renderer
// to browser
template <typename PageHandler, typename Page>
class OobeMojoBinder<PageHandler, Page> {
 public:
  explicit OobeMojoBinder(PageHandler* impl) : page_handler_receiver_(impl) {
    page_pending_receiver_ = page_remote_.BindNewPipeAndPassReceiver();
  }

  void BindPageHandlerReceiver(
      mojo::PendingReceiver<PageHandler> pending_receiver) {
    CHECK(!page_handler_receiver_.is_bound());
    page_handler_receiver_.Bind(std::move(pending_receiver));
  }

  mojo::Remote<Page>* GetRemote() { return &page_remote_; }

  void PassPagePendingReceiverWithCallback(
      base::OnceCallback<void(mojo::PendingReceiver<Page>)> callback) {
    std::move(callback).Run(std::move(page_pending_receiver_));
  }

 private:
  mojo::PendingReceiver<Page> page_pending_receiver_;
  mojo::Remote<Page> page_remote_;
  mojo::Receiver<PageHandler> page_handler_receiver_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OOBE_MOJO_BINDER_H_
