// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_data_observer.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace glic {

class GlicTabDataObserver::TabObserver : public content::WebContentsObserver {
 public:
  TabObserver(GlicTabDataObserver* owner_observer, tabs::TabInterface* tab)
      : content::WebContentsObserver(tab->GetContents()),
        owner_observer_(owner_observer),
        tab_(tab) {
    tab_data_receivers_.set_disconnect_handler(base::BindRepeating(
        &TabObserver::Disconnected, base::Unretained(this)));
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
        base::BindRepeating(&TabObserver::SendTabData, base::Unretained(this)));

    UpdateWindowObservations();
  }

  ~TabObserver() override = default;

  void Subscribe(::mojo::PendingRemote<mojom::TabDataHandler> receiver) {
    mojo::Remote<mojom::TabDataHandler> new_remote;
    new_remote.Bind(std::move(receiver));
    new_remote->OnTabDataChanged(CreateTabData(tab_));
    tab_data_receivers_.Add(std::move(new_remote));
  }

  bool HasReceivers() const { return !tab_data_receivers_.empty(); }

 private:
  void Disconnected(mojo::RemoteSetElementId element_id) {
    if (tab_data_receivers_.empty()) {
      owner_observer_->ScheduleCleanupForTab(tab_->GetHandle());
    }
  }

  void UpdateWindowObservations() {
#if !BUILDFLAG(IS_ANDROID)
    BrowserWindowInterface* browser_window = tab_->GetBrowserWindowInterface();
    if (!browser_window) {
      return;
    }
    window_did_become_active_subscription_ =
        browser_window->RegisterDidBecomeActive(base::BindRepeating(
            &TabObserver::HandleWindowActivatedChange, base::Unretained(this)));
    window_did_become_inactive_subscription_ =
        browser_window->RegisterDidBecomeInactive(base::BindRepeating(
            &TabObserver::HandleWindowActivatedChange, base::Unretained(this)));
#endif
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
    SendTabData(
        TabDataChange{{TabDataChangeCause::kVisibility}, CreateTabData(tab_)});
  }

#if !BUILDFLAG(IS_ANDROID)
  // Callback for BrowserWindowInterface activated changes.
  void HandleWindowActivatedChange(BrowserWindowInterface* browser_window) {
    SendTabData(
        TabDataChange{{TabDataChangeCause::kVisibility}, CreateTabData(tab_)});
  }
#endif

  void OnDidInsert(tabs::TabInterface* tab) { UpdateWindowObservations(); }

  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason) {
    if (reason == tabs::TabInterface::DetachReason::kDelete) {
      // Destroys `this`.
      owner_observer_->OnTabWillClose(tab->GetHandle());
    }
  }

  void SendTabData(TabDataChange tab_data) {
    for (auto& receiver : tab_data_receivers_) {
      receiver->OnTabDataChanged(tab_data.tab_data->Clone());
    }
  }

  // Owns this.
  raw_ptr<GlicTabDataObserver> owner_observer_;

  raw_ptr<tabs::TabInterface> tab_;

  mojo::RemoteSet<mojom::TabDataHandler> tab_data_receivers_;

  base::CallbackListSubscription did_insert_subscription_;
  base::CallbackListSubscription will_detach_subscription_;

  // Subscriptions for changes to TabInterface::IsActivated.
  base::CallbackListSubscription tab_did_activate_subscription_;
  base::CallbackListSubscription tab_will_deactivate_subscription_;

#if !BUILDFLAG(IS_ANDROID)
  // Subscriptions for changes to BrowserWindowInterface::IsActive.
  base::CallbackListSubscription window_did_become_active_subscription_;
  base::CallbackListSubscription window_did_become_inactive_subscription_;
#endif

  std::unique_ptr<TabDataObserver> tab_data_observer_;

  base::WeakPtrFactory<TabObserver> weak_ptr_factory_{this};
};

GlicTabDataObserver::GlicTabDataObserver() = default;
GlicTabDataObserver::~GlicTabDataObserver() = default;

void GlicTabDataObserver::OnTabWillClose(tabs::TabHandle tab_handle) {
  observers_.erase(tab_handle);
}

void GlicTabDataObserver::SubscribeToTabData(
    int32_t tab_id,
    mojo::PendingRemote<mojom::TabDataHandler> remote) {
  tabs::TabInterface::Handle handle(tab_id);
  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    remote.reset();
    return;
  }
  TabObserver* observer_ptr = nullptr;
  auto iter = observers_.find(handle);
  if (iter != observers_.end()) {
    observer_ptr = iter->second.get();
  } else {
    auto observer = std::make_unique<TabObserver>(this, tab);
    observer_ptr = observer.get();
    observers_.insert({handle, std::move(observer)});
  }
  observer_ptr->Subscribe(std::move(remote));
}

// Schedules deleting the tab observer later. This isn't done immediately
// to avoid teardown if the observer is used again quickly.
void GlicTabDataObserver::ScheduleCleanupForTab(tabs::TabHandle tab_handle) {
  pending_cleanup_.insert(tab_handle);
  if (cleanup_timer_.IsRunning()) {
    return;
  }
  cleanup_timer_.Start(
      FROM_HERE, base::Seconds(5),
      base::BindOnce(&GlicTabDataObserver::DoCleanup, base::Unretained(this)));
}

void GlicTabDataObserver::DoCleanup() {
  for (tabs::TabHandle handle : std::exchange(pending_cleanup_, {})) {
    auto iter = observers_.find(handle);
    if (iter != observers_.end()) {
      if (!iter->second->HasReceivers()) {
        observers_.erase(iter);
      }
    }
  }
}

}  // namespace glic
