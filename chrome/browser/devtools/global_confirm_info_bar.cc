// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/global_confirm_info_bar.h"

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/infobars/core/infobar.h"
#include "ui/gfx/image/image.h"

// InfoBarDelegateProxy -------------------------------------------------------

class GlobalConfirmInfoBar::DelegateProxy : public ConfirmInfoBarDelegate {
 public:
  explicit DelegateProxy(base::WeakPtr<GlobalConfirmInfoBar> global_info_bar);
  ~DelegateProxy() override;
  void Detach();

 private:
  friend class GlobalConfirmInfoBar;

  // ConfirmInfoBarDelegate overrides
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  base::string16 GetMessageText() const override;
  gfx::ElideBehavior GetMessageElideBehavior() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  GURL GetLinkURL() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  void InfoBarDismissed() override;

  infobars::InfoBar* info_bar_;
  base::WeakPtr<GlobalConfirmInfoBar> global_info_bar_;

  DISALLOW_COPY_AND_ASSIGN(DelegateProxy);
};

GlobalConfirmInfoBar::DelegateProxy::DelegateProxy(
    base::WeakPtr<GlobalConfirmInfoBar> global_info_bar)
    : info_bar_(nullptr),
      global_info_bar_(global_info_bar) {
}

GlobalConfirmInfoBar::DelegateProxy::~DelegateProxy() {
}

infobars::InfoBarDelegate::InfoBarIdentifier
GlobalConfirmInfoBar::DelegateProxy::GetIdentifier() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetIdentifier()
                          : INVALID;
}

base::string16 GlobalConfirmInfoBar::DelegateProxy::GetMessageText() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetMessageText()
                          : base::string16();
}

gfx::ElideBehavior
GlobalConfirmInfoBar::DelegateProxy::GetMessageElideBehavior() const {
  return global_info_bar_
             ? global_info_bar_->delegate_->GetMessageElideBehavior()
             : ConfirmInfoBarDelegate::GetMessageElideBehavior();
}

int GlobalConfirmInfoBar::DelegateProxy::GetButtons() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetButtons()
                          : 0;
}

base::string16 GlobalConfirmInfoBar::DelegateProxy::GetButtonLabel(
    InfoBarButton button) const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetButtonLabel(button)
                          : base::string16();
}

bool GlobalConfirmInfoBar::DelegateProxy::Accept() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  // Remove the current InfoBar (the one whose Accept button is being clicked)
  // from the control of GlobalConfirmInfoBar. This InfoBar will be closed by
  // caller of this method, and we don't need GlobalConfirmInfoBar to close it.
  // Furthermore, letting GlobalConfirmInfoBar close the current InfoBar can
  // cause memory corruption when InfoBar animation is disabled.
  if (info_bar) {
    info_bar->OnInfoBarRemoved(info_bar_, false);
    info_bar->delegate_->Accept();
  }
  // Could be destroyed after this point.
  if (info_bar)
      info_bar->Close();
  return true;
}

bool GlobalConfirmInfoBar::DelegateProxy::Cancel() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  // See comments in GlobalConfirmInfoBar::DelegateProxy::Accept().
  if (info_bar) {
    info_bar->OnInfoBarRemoved(info_bar_, false);
    info_bar->delegate_->Cancel();
  }
  // Could be destroyed after this point.
  if (info_bar)
      info_bar->Close();
  return true;
}

base::string16 GlobalConfirmInfoBar::DelegateProxy::GetLinkText() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetLinkText()
                          : base::string16();
}

GURL GlobalConfirmInfoBar::DelegateProxy::GetLinkURL() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetLinkURL()
                          : GURL();
}

bool GlobalConfirmInfoBar::DelegateProxy::LinkClicked(
    WindowOpenDisposition disposition) {
  return global_info_bar_ ?
      global_info_bar_->delegate_->LinkClicked(disposition) : false;
}

void GlobalConfirmInfoBar::DelegateProxy::InfoBarDismissed() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  // See comments in GlobalConfirmInfoBar::DelegateProxy::Accept().
  if (info_bar) {
    info_bar->OnInfoBarRemoved(info_bar_, false);
    info_bar->delegate_->InfoBarDismissed();
  }
  // Could be destroyed after this point.
  if (info_bar)
      info_bar->Close();
}

void GlobalConfirmInfoBar::DelegateProxy::Detach() {
  global_info_bar_.reset();
}

// GlobalConfirmInfoBar -------------------------------------------------------

// static
base::WeakPtr<GlobalConfirmInfoBar> GlobalConfirmInfoBar::Show(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  GlobalConfirmInfoBar* info_bar =
      new GlobalConfirmInfoBar(std::move(delegate));
  return info_bar->weak_factory_.GetWeakPtr();
}

void GlobalConfirmInfoBar::Close() {
  delete this;
}

GlobalConfirmInfoBar::GlobalConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : delegate_(std::move(delegate)),
      browser_tab_strip_tracker_(this, nullptr, nullptr),
      is_closing_(false) {
  browser_tab_strip_tracker_.Init();
}

GlobalConfirmInfoBar::~GlobalConfirmInfoBar() {
  while (!proxies_.empty()) {
    auto it = proxies_.begin();
    it->second->Detach();
    it->first->RemoveObserver(this);
    it->first->RemoveInfoBar(it->second->info_bar_);
    proxies_.erase(it);
  }
}

void GlobalConfirmInfoBar::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted)
    return;
  for (const auto& contents : change.GetInsert()->contents)
    MaybeAddInfoBar(contents.contents);
}

void GlobalConfirmInfoBar::TabChangedAt(content::WebContents* web_contents,
                                        int index,
                                        TabChangeType change_type) {
  MaybeAddInfoBar(web_contents);
}

void GlobalConfirmInfoBar::OnInfoBarRemoved(infobars::InfoBar* info_bar,
                                            bool animate) {
  // Do not process alien infobars.
  for (auto it : proxies_) {
    if (it.second->info_bar_ == info_bar) {
      OnManagerShuttingDown(info_bar->owner());
      break;
    }
  }
}

void GlobalConfirmInfoBar::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  manager->RemoveObserver(this);
  proxies_.erase(manager);
}

void GlobalConfirmInfoBar::MaybeAddInfoBar(content::WebContents* web_contents) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  // WebContents from the tab strip must have the infobar service.
  DCHECK(infobar_service);
  if (base::Contains(proxies_, infobar_service))
    return;

  std::unique_ptr<GlobalConfirmInfoBar::DelegateProxy> proxy(
      new GlobalConfirmInfoBar::DelegateProxy(weak_factory_.GetWeakPtr()));
  GlobalConfirmInfoBar::DelegateProxy* proxy_ptr = proxy.get();
  infobars::InfoBar* added_bar = infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(std::move(proxy)));

  // If AddInfoBar() fails, either infobars are globally disabled, or something
  // strange has gone wrong and we can't show the infobar on every tab. In
  // either case, it doesn't make sense to keep the global object open,
  // especially since some callers expect it to delete itself when a user acts
  // on the underlying infobars.
  //
  // Asynchronously delete the global object because the BrowserTabStripTracker
  // doesn't support being deleted while iterating over the existing tabs.
  if (!added_bar) {
    if (!is_closing_) {
      is_closing_ = true;

      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&GlobalConfirmInfoBar::Close,
                                    weak_factory_.GetWeakPtr()));
    }
    return;
  }

  proxy_ptr->info_bar_ = added_bar;
  proxies_[infobar_service] = proxy_ptr;
  infobar_service->AddObserver(this);
}
