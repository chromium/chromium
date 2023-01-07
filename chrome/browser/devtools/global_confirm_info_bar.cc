// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/global_confirm_info_bar.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/infobars/core/infobar.h"
#include "ui/gfx/image/image.h"

class GlobalConfirmInfoBar::DelegateProxy : public ConfirmInfoBarDelegate {
 public:
  explicit DelegateProxy(base::WeakPtr<GlobalConfirmInfoBar> global_info_bar);

  DelegateProxy(const DelegateProxy&) = delete;
  DelegateProxy& operator=(const DelegateProxy&) = delete;

  ~DelegateProxy() override;
  void Detach();

 private:
  friend class GlobalConfirmInfoBar;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  std::u16string GetLinkText() const override;
  GURL GetLinkURL() const override;
  void InfoBarDismissed() override;
  std::u16string GetMessageText() const override;
  gfx::ElideBehavior GetMessageElideBehavior() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  bool IsCloseable() const override;
  bool ShouldAnimate() const override;

  base::WeakPtr<GlobalConfirmInfoBar> global_info_bar_;
};

GlobalConfirmInfoBar::DelegateProxy::DelegateProxy(
    base::WeakPtr<GlobalConfirmInfoBar> global_info_bar)
    : global_info_bar_(global_info_bar) {}

GlobalConfirmInfoBar::DelegateProxy::~DelegateProxy() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
GlobalConfirmInfoBar::DelegateProxy::GetIdentifier() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetIdentifier()
                          : INVALID;
}

std::u16string GlobalConfirmInfoBar::DelegateProxy::GetLinkText() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetLinkText()
                          : ConfirmInfoBarDelegate::GetLinkText();
}

GURL GlobalConfirmInfoBar::DelegateProxy::GetLinkURL() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetLinkURL()
                          : ConfirmInfoBarDelegate::GetLinkURL();
}

bool GlobalConfirmInfoBar::DelegateProxy::IsCloseable() const {
  return global_info_bar_ ? global_info_bar_->delegate_->IsCloseable()
                          : ConfirmInfoBarDelegate::IsCloseable();
}

bool GlobalConfirmInfoBar::DelegateProxy::ShouldAnimate() const {
  return global_info_bar_ ? global_info_bar_->delegate_->ShouldAnimate()
                          : ConfirmInfoBarDelegate::ShouldAnimate();
}

void GlobalConfirmInfoBar::DelegateProxy::InfoBarDismissed() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  // Remove the current InfoBar (the one whose close button is being clicked)
  // from the control of GlobalConfirmInfoBar. This InfoBar will be closed by
  // caller of this method, and we don't need GlobalConfirmInfoBar to close it.
  // Furthermore, letting GlobalConfirmInfoBar close the current InfoBar can
  // cause memory corruption when InfoBar animation is disabled.
  if (info_bar) {
    info_bar->OnInfoBarRemoved(infobar(), false);
    info_bar->delegate_->InfoBarDismissed();
    // Check the pointer again in case it's now destroyed.
    // TODO(pkasting): We should audit callees for these sorts of methods
    // (InfoBarDismissed(), Accept(), Cancel()) to determine if they can close
    // the global infobar, then establish better contracts/APIs around the
    // lifetimes here, ideally removing WeakPtrs entirely.
    if (info_bar)
      info_bar->Close();
  } else {
    ConfirmInfoBarDelegate::InfoBarDismissed();
  }
}

std::u16string GlobalConfirmInfoBar::DelegateProxy::GetMessageText() const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetMessageText()
                          : std::u16string();
}

gfx::ElideBehavior
GlobalConfirmInfoBar::DelegateProxy::GetMessageElideBehavior() const {
  return global_info_bar_
             ? global_info_bar_->delegate_->GetMessageElideBehavior()
             : ConfirmInfoBarDelegate::GetMessageElideBehavior();
}

int GlobalConfirmInfoBar::DelegateProxy::GetButtons() const {
  // ConfirmInfoBarDelegate default behavior here is not very good for a no-op
  // case, so return BUTTON_NONE when there is no underlying delegate.
  return global_info_bar_ ? global_info_bar_->delegate_->GetButtons()
                          : BUTTON_NONE;
}

std::u16string GlobalConfirmInfoBar::DelegateProxy::GetButtonLabel(
    InfoBarButton button) const {
  return global_info_bar_ ? global_info_bar_->delegate_->GetButtonLabel(button)
                          : ConfirmInfoBarDelegate::GetButtonLabel(button);
}

bool GlobalConfirmInfoBar::DelegateProxy::Accept() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  // See comments in InfoBarDismissed().
  if (info_bar) {
    // TODO(pkasting): This implementation assumes the global delegate's
    // Accept() always returns true.  Ideally, we'd check the return value and
    // handle it appropriately.  We also need to worry about side effects like
    // navigating the current tab and whether that can corrupt state or result
    // in double-frees.
    info_bar->OnInfoBarRemoved(infobar(), false);
    info_bar->delegate_->Accept();
    if (info_bar)
      info_bar->Close();
    return true;
  }
  return ConfirmInfoBarDelegate::Accept();
}

bool GlobalConfirmInfoBar::DelegateProxy::Cancel() {
  base::WeakPtr<GlobalConfirmInfoBar> info_bar = global_info_bar_;
  // See comments in InfoBarDismissed().
  if (info_bar) {
    // See comments in Accept().
    info_bar->OnInfoBarRemoved(infobar(), false);
    info_bar->delegate_->Cancel();
    if (info_bar)
      info_bar->Close();
    return true;
  }
  return ConfirmInfoBarDelegate::Cancel();
}

void GlobalConfirmInfoBar::DelegateProxy::Detach() {
  global_info_bar_.reset();
}

// static
GlobalConfirmInfoBar* GlobalConfirmInfoBar::Show(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  // Owns itself, deleted by Close().
  return new GlobalConfirmInfoBar(std::move(delegate));
}

GlobalConfirmInfoBar::GlobalConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : delegate_(std::move(delegate)) {
  browser_tab_strip_tracker_.Init();
}

GlobalConfirmInfoBar::~GlobalConfirmInfoBar() {
  while (!proxies_.empty()) {
    auto it = proxies_.begin();
    it->second->Detach();
    it->first->RemoveObserver(this);
    it->first->RemoveInfoBar(it->second->infobar());
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
  for (const auto& it : proxies_) {
    if (it.second->infobar() == info_bar) {
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

void GlobalConfirmInfoBar::Close() {
  delete this;
}

void GlobalConfirmInfoBar::MaybeAddInfoBar(content::WebContents* web_contents) {
  if (is_closing_)
    return;

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  // WebContents from the tab strip must have the infobar manager.
  DCHECK(infobar_manager);
  if (base::Contains(proxies_, infobar_manager))
    return;

  auto proxy = std::make_unique<GlobalConfirmInfoBar::DelegateProxy>(
      weak_factory_.GetWeakPtr());
  GlobalConfirmInfoBar::DelegateProxy* proxy_ptr = proxy.get();
  infobars::InfoBar* added_bar =
      infobar_manager->AddInfoBar(CreateConfirmInfoBar(std::move(proxy)));

  // If AddInfoBar() fails, either infobars are globally disabled, or something
  // strange has gone wrong and we can't show the infobar on every tab. In
  // either case, it doesn't make sense to keep the global object open,
  // especially since some callers expect it to delete itself when a user acts
  // on the underlying infobars.
  //
  // Asynchronously delete the global object because the BrowserTabStripTracker
  // doesn't support being deleted while iterating over the existing tabs.
  if (!added_bar) {
    is_closing_ = true;

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&GlobalConfirmInfoBar::Close,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  proxies_[infobar_manager] = proxy_ptr;
  infobar_manager->AddObserver(this);
}
