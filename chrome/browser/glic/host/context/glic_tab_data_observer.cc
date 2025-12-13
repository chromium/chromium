// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_data_observer.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents_observer.h"

namespace glic {

class GlicTabDataObserver::TabObserver : public content::WebContentsObserver {
 public:
  TabObserver(GlicTabDataObserver* owner_observer, tabs::TabInterface* tab)
      : content::WebContentsObserver(tab->GetContents()),
        owner_observer_(owner_observer),
        tab_(tab) {
    did_insert_subscription_ = tab_->RegisterDidInsert(
        base::BindRepeating(&TabObserver::OnDidInsert, base::Unretained(this)));
    will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
        &TabObserver::OnWillDetach, base::Unretained(this)));
    tab_did_activate_subscription_ =
        tab_->RegisterDidActivate(base::BindRepeating(
            &TabObserver::HandleTabActivatedChange, base::Unretained(this)));
    tab_will_deactivate_subscription_ =
        tab_->RegisterWillDeactivate(base::BindRepeating(
            &TabObserver::HandleTabActivatedChange, base::Unretained(this)));
    tab_data_observer_ = std::make_unique<TabDataObserver>(
        tab_, tab_->GetContents(),
        base::BindRepeating(&TabObserver::TabDataChanged,
                            base::Unretained(this)));

    UpdateWindowObservations();

    TabDataChanged(TabDataChange{/*causes=*/{}, CreateTabData(web_contents())});
  }

  ~TabObserver() override = default;

 private:
  void UpdateWindowObservations() {
    BrowserWindowInterface* browser_window = tab_->GetBrowserWindowInterface();
    window_did_become_active_subscription_ =
        browser_window->RegisterDidBecomeActive(base::BindRepeating(
            &TabObserver::HandleWindowActivatedChange, base::Unretained(this)));
    window_did_become_inactive_subscription_ =
        browser_window->RegisterDidBecomeInactive(base::BindRepeating(
            &TabObserver::HandleWindowActivatedChange, base::Unretained(this)));
  }

  // Callback for TabInterface activated changes.
  void HandleTabActivatedChange(tabs::TabInterface* tab) {
    // If this is for deactivation, the change happens after returning, so we
    // handle this asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TabObserver::NotifyTabInfoChangeAfterTabActivatedChange,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Runs asynchronously after HandleTabActivatedChange, once the changes
  // actually take effect.
  void NotifyTabInfoChangeAfterTabActivatedChange() {
    TabDataChanged(TabDataChange{{TabDataChangeCause::kVisibility},
                                 CreateTabData(web_contents())});
  }

  // Callback for BrowserWindowInterface activated changes.
  void HandleWindowActivatedChange(BrowserWindowInterface* browser_window) {
    TabDataChanged(TabDataChange{{TabDataChangeCause::kVisibility},
                                 CreateTabData(web_contents())});
  }

  void OnDidInsert(tabs::TabInterface* tab) { UpdateWindowObservations(); }

  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason) {
    if (reason == tabs::TabInterface::DetachReason::kDelete) {
      // Destroys `this`.
      owner_observer_->OnTabWillClose(tab->GetHandle());
    }
  }

  void TabDataChanged(TabDataChange tab_data) {
    SendTabData(std::move(tab_data));
  }

  void SendTabData(TabDataChange tab_data) {
    owner_observer_->OnTabDataChanged(std::move(tab_data));
  }

  // Owns this.
  raw_ptr<GlicTabDataObserver> owner_observer_;

  raw_ptr<tabs::TabInterface> tab_;

  base::CallbackListSubscription did_insert_subscription_;
  base::CallbackListSubscription will_detach_subscription_;

  // Subscriptions for changes to TabInterface::IsActivated.
  base::CallbackListSubscription tab_did_activate_subscription_;
  base::CallbackListSubscription tab_will_deactivate_subscription_;

  // Subscriptions for changes to BrowserWindowInterface::IsActive.
  base::CallbackListSubscription window_did_become_active_subscription_;
  base::CallbackListSubscription window_did_become_inactive_subscription_;

  std::unique_ptr<TabDataObserver> tab_data_observer_;

  base::WeakPtrFactory<TabObserver> weak_ptr_factory_{this};
};

GlicTabDataObserver::GlicTabDataObserver() = default;
GlicTabDataObserver::~GlicTabDataObserver() = default;

base::CallbackListSubscription GlicTabDataObserver::AddTabDataChangedCallback(
    TabDataChangedCallback callback) {
  return tab_data_changed_callback_list_.Add(std::move(callback));
}

void GlicTabDataObserver::ObserveTabData(tabs::TabHandle tab_handle) {
  if (!base::FeatureList::IsEnabled(features::kGlicGetTabByIdApi)) {
    return;
  }
  auto* tab = tab_handle.Get();
  if (!tab) {
    return;
  }

  if (!observers_.contains(tab_handle)) {
    observers_.insert({tab_handle, std::make_unique<TabObserver>(this, tab)});
  }
}

void GlicTabDataObserver::OnTabWillClose(tabs::TabHandle tab_handle) {
  observers_.erase(tab_handle);
}

void GlicTabDataObserver::OnTabDataChanged(TabDataChange tab_data) {
  tab_data_changed_callback_list_.Notify(tab_data);
}

}  // namespace glic
