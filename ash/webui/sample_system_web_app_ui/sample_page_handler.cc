// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/sample_system_web_app_ui/sample_page_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/thread_pool.h"

namespace ash {

PageHandler::PageHandler() = default;
PageHandler::~PageHandler() = default;

void PageHandler::BindInterface(
    mojo::PendingReceiver<mojom::sample_swa::PageHandler> pending_receiver,
    mojo::PendingRemote<mojom::sample_swa::Page> pending_page) {
  receiver_.Bind(std::move(pending_receiver));
  page_.Bind(std::move(pending_page));

  on_page_handler_bound_.Signal();
}

void PageHandler::GetPreferences(GetPreferencesCallback callback) {
  // Returns hardcoded preferences. In a real application this would
  // be done with the help of Preference APIs.
  std::move(callback).Run(
      mojom::sample_swa::Preferences::New(/*background=*/"#ffffff",
                                          /*foreground=*/"#000000"));
}

void PageHandler::Send(const std::string& message) {
  message_ = message;
  VLOG(1) << "Message set: " << message;
  // In real application we would do something with the received value.
}

void PageHandler::DoSomething() {
  // Pretends to do some work. In reality it just posts a task that will,
  // once completed, issue an event notification to the page. We use a weak
  // ptr to the PageHandler object, as the thread pool's lifetime may
  // be longer than that of PageHandler.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, base::BindOnce([]() {
        // Do some work here.
      }),
      base::BindOnce(&PageHandler::OnSomethingDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PageHandler::OnSomethingDone() {
  page_->OnEventOccurred("DoSomething is done");
}

void PageHandler::CreateParentPage(
    mojo::PendingRemote<mojom::sample_swa::ChildUntrustedPage>
        child_trusted_page,
    mojo::PendingReceiver<mojom::sample_swa::ParentTrustedPage>
        parent_trusted_page) {
  on_page_handler_bound_.Post(
      FROM_HERE,
      base::BindOnce(&PageHandler::BindChildPageInJavaScript,
                     // Safe to base::Unretained(), `this` owns
                     // `on_page_handler_ready_`. The callback
                     // won't be invoked if `this` is destroyed.
                     base::Unretained(this), std::move(child_trusted_page),
                     std::move(parent_trusted_page)));
}

void PageHandler::BindChildPageInJavaScript(
    mojo::PendingRemote<mojom::sample_swa::ChildUntrustedPage>
        child_trusted_page,
    mojo::PendingReceiver<mojom::sample_swa::ParentTrustedPage>
        parent_trusted_page) {
  page_->CreateParentPage(std::move(child_trusted_page),
                          std::move(parent_trusted_page));
}

}  // namespace ash
