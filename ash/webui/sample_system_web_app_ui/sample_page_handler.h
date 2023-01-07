// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_PAGE_HANDLER_H_
#define ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_PAGE_HANDLER_H_

#include <memory>

#include "ash/webui/sample_system_web_app_ui/mojom/sample_system_web_app_ui.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/string16.mojom.h"

namespace ash {

// Implements the PageHandler interface. For illustration purposes this
// class also interacts with a remote Page. There are five examples of
// interaction.
//  1. The UI fetches "preferences" (an example of "get")
//  2. The UI sends a message, (an example of "set")
//  3. The UI asks the PageHandler to execute an action (DoSomething).
//  4. The PageHandler notifies the page about an event.
class PageHandler : public mojom::sample_swa::PageHandler {
 public:
  PageHandler();
  ~PageHandler() override;

  PageHandler(const PageHandler&) = delete;
  PageHandler& operator=(const PageHandler&) = delete;

  // Called when a child page wants to bind its interface in `page_`, so they
  // can communicate via Mojo.
  void CreateParentPage(
      mojo::PendingRemote<mojom::sample_swa::ChildUntrustedPage>
          child_untrusted_page,
      mojo::PendingReceiver<mojom::sample_swa::ParentTrustedPage>
          parent_trusted_page);

  void BindInterface(
      mojo::PendingReceiver<mojom::sample_swa::PageHandler> pending_receiver,
      mojo::PendingRemote<mojom::sample_swa::Page> pending_page);

 private:
  // Shows how the page can retrieve information from the browser process.
  void GetPreferences(GetPreferencesCallback callback) override;

  // Shows how the page can send information to the browser process.
  void Send(const std::string& message) override;

  // Handles the page requesting an action from the controller.
  void DoSomething() override;

  // Called as a reaction to DoSomething; invoked when DoSomething is done.
  void OnSomethingDone();

  // Called after `page_` is bound. This requests `page_` to bind child page's
  // Mojo pipes in JavaScript.
  void BindChildPageInJavaScript(
      mojo::PendingRemote<mojom::sample_swa::ChildUntrustedPage>
          child_trusted_page,
      mojo::PendingReceiver<mojom::sample_swa::ParentTrustedPage>
          parent_trusted_page);

  mojo::Receiver<mojom::sample_swa::PageHandler> receiver_{this};
  mojo::Remote<mojom::sample_swa::Page> page_;

  std::string message_;

  // The event signaling page handler is bound. This is the earliest time `this`
  // can send Mojo messages to JavaScript.
  base::OneShotEvent on_page_handler_bound_;

  base::WeakPtrFactory<PageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_SAMPLE_SYSTEM_WEB_APP_UI_SAMPLE_PAGE_HANDLER_H_
