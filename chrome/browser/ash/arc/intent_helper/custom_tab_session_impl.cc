// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/intent_helper/custom_tab_session_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/arc/intent_helper/custom_tab.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"

// static
mojo::PendingRemote<arc::mojom::CustomTabSession> CustomTabSessionImpl::Create(
    std::unique_ptr<arc::CustomTab> custom_tab,
    Browser* browser) {
  DCHECK(custom_tab);

  // This object will be deleted when the mojo connection is closed.
  auto* tab = new CustomTabSessionImpl(std::move(custom_tab), browser);
  mojo::PendingRemote<arc::mojom::CustomTabSession> remote;
  tab->Bind(&remote);
  return remote;
}

CustomTabSessionImpl::CustomTabSessionImpl(
    std::unique_ptr<arc::CustomTab> custom_tab,
    Browser* browser)
    : browser_(browser),
      custom_tab_(std::move(custom_tab)),
      weak_ptr_factory_(this) {
  aura::Window* native_view = browser_->window()->GetNativeWindow();
  custom_tab_->Attach(native_view);
  browser_->window()->Show();
  browser_->tab_strip_model()->AddObserver(this);
}

CustomTabSessionImpl::~CustomTabSessionImpl() {
  if (!browser_)
    return;

  auto* tab_strip_model = browser_->tab_strip_model();
  DCHECK(tab_strip_model);
  tab_strip_model->RemoveObserver(this);
  int index = tab_strip_model->GetIndexOfWebContents(
      tab_strip_model->GetActiveWebContents());
  tab_strip_model->DetachAndDeleteWebContentsAt(index);
}

void CustomTabSessionImpl::OnOpenInChromeClicked() {
  forwarded_to_normal_tab_ = true;
}

void CustomTabSessionImpl::Bind(
    mojo::PendingRemote<arc::mojom::CustomTabSession>* remote) {
  receiver_.Bind(remote->InitWithNewPipeAndPassReceiver());
  receiver_.set_disconnect_handler(base::BindOnce(
      &CustomTabSessionImpl::Close, weak_ptr_factory_.GetWeakPtr()));
}

// Deletes this object when the mojo connection is closed.
void CustomTabSessionImpl::Close() {
  delete this;
}

// This should only be called once because a custom tab is a single tabbed
// browser.
void CustomTabSessionImpl::TabStripEmpty() {
  browser_->tab_strip_model()->RemoveObserver(this);
  browser_ = nullptr;
  forwarded_to_normal_tab_ = true;
  delete this;
}
