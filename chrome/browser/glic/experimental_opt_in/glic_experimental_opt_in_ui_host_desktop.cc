// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace glic {

namespace {

// Returns whether the provided `url` matches any of the `target_hosts` and its
// path starts with any of the `target_path_starts`
bool DoesURLMatchTarget(const GURL& url,
                        const std::vector<std::string>& target_hosts,
                        const std::vector<std::string>& target_path_starts) {
  if (!url.is_valid()) {
    return false;
  }

  std::string_view host = url.host();
  if (std::find(target_hosts.begin(), target_hosts.end(), host) ==
      target_hosts.end()) {
    return false;
  }

  std::string_view path = url.path();
  for (const auto& path_start : target_path_starts) {
    if (path.starts_with(path_start)) {
      return true;
    }
  }
  return false;
}

}  // namespace

class GlicExperimentalOptInUIHostDesktop : public GlicExperimentalOptInUIHost {
 public:
  GlicExperimentalOptInUIHostDesktop(Profile* profile, Delegate* delegate)
      : profile_(profile),
        delegate_(delegate),
        tick_clock_(base::DefaultTickClock::GetInstance()) {}

  ~GlicExperimentalOptInUIHostDesktop() override {
    delegate_ = nullptr;
    Close(/*accepted=*/false);
  }

  void Show(content::WebContents* web_contents) override {
    if (dialog_widget_) {
      dialog_widget_->Show();
      return;
    }

    tabs::TabInterface* tab_interface =
        tabs::TabInterface::MaybeGetFromContents(web_contents);
    if (!tab_interface) {
      if (delegate_) {
        delegate_->OnUIClosed(/*accepted=*/false);
      }
      return;
    }

    tab_interface_ = tab_interface->GetWeakPtr();

    // Focus the tab and window so the user sees the dialog.
    if (auto* window = tab_interface->GetBrowserWindowInterface()) {
      auto* tab_list = TabListInterface::From(window);
      tab_list->ActivateTab(tab_interface->GetHandle());
      window->GetWindow()->Show();
      window->GetWindow()->Activate();
    }

    dialog_view_ = std::make_unique<GlicExperimentalOptInDialogView>(
        profile_, tab_interface);
    dialog_view_->SetOwnershipOfNewWidget(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);

    auto params = std::make_unique<tabs::TabDialogManager::Params>();
    params->close_on_navigate = false;
    dialog_widget_ =
        tab_interface->GetTabFeatures()
            ->tab_dialog_manager()
            ->CreateAndShowDialog(dialog_view_.get(), std::move(params));

    dialog_open_time_ = tick_clock_->NowTicks();
    if (tab_interface->IsVisible()) {
      visibility_start_time_ = dialog_open_time_;
    }

    tab_subscriptions_.push_back(
        tab_interface->RegisterDidBecomeVisible(base::BindRepeating(
            &GlicExperimentalOptInUIHostDesktop::TabDidBecomeVisible,
            weak_ptr_factory_.GetWeakPtr())));
    tab_subscriptions_.push_back(
        tab_interface->RegisterWillBecomeHidden(base::BindRepeating(
            &GlicExperimentalOptInUIHostDesktop::TabWillBecomeHidden,
            weak_ptr_factory_.GetWeakPtr())));

    dialog_widget_->MakeCloseSynchronous(
        base::BindOnce(&GlicExperimentalOptInUIHostDesktop::CloseWidget,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void Close(bool accepted) override {
    CloseWidget(accepted ? views::Widget::ClosedReason::kAcceptButtonClicked
                         : views::Widget::ClosedReason::kCancelButtonClicked);
  }

  void OpenLinkInNewTab(const GURL& url) override {
    if (!tab_interface_) {
      return;
    }
    if (auto* browser_window = tab_interface_->GetBrowserWindowInterface()) {
      browser_window->OpenGURL(url, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    }
  }

  content::WebContents* GetOrCreateSuitableWebContents() override {
    // 1. Prepare target hosts and path from feature params.
    std::string allowed_hosts_str =
        features::kGlicExperimentalTriggeringTabFocusHosts.Get();
    std::vector<std::string> target_hosts =
        base::SplitString(allowed_hosts_str, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    std::string target_paths_str =
        features::kGlicExperimentalTriggeringTabFocusPathSubstring.Get();
    std::vector<std::string> target_paths =
        base::SplitString(target_paths_str, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);

    // 2. Ensure we have at least one window.
    auto* collection = ProfileBrowserCollection::GetForProfile(profile_);
    if (collection->IsEmpty()) {
      if (base::FeatureList::IsEnabled(
              features::kGlicExperimentalTriggeringOpenWindowIfNone)) {
        chrome::OpenEmptyWindow(profile_);
      }
    }

    if (collection->IsEmpty()) {
      return nullptr;
    }

    // 3. Search all windows for this profile in activation order, so we
    // prioritize the currently active window.
    content::WebContents* suitable_contents = nullptr;

    collection->ForEach(
        [&suitable_contents, &target_hosts,
         &target_paths](BrowserWindowInterface* window) {
          if (window->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
            return true;
          }
          auto* tab_list = TabListInterface::From(window);
          // Check active tab first
          if (auto* active_tab = tab_list->GetActiveTab()) {
            if (DoesURLMatchTarget(active_tab->GetURL(), target_hosts,
                                   target_paths)) {
              suitable_contents = active_tab->GetContents();
              return false;  // Stop iteration
            }
          }
          // Check other tabs
          for (tabs::TabInterface* tab : tab_list->GetAllTabs()) {
            if (tab == tab_list->GetActiveTab()) {
              continue;
            }
            if (DoesURLMatchTarget(tab->GetURL(), target_hosts, target_paths)) {
              suitable_contents = tab->GetContents();
              return false;  // Stop iteration
            }
          }
          return true;  // Continue iteration
        },
        BrowserCollection::Order::kActivation);

    if (suitable_contents) {
      return suitable_contents;
    }

    // 4. No suitable tab found. Open a new one in the last active window.
    BrowserWindowInterface* window_to_open_in =
        collection->GetLastActiveBrowser();
    CHECK(window_to_open_in);

    auto* tab_list = TabListInterface::From(window_to_open_in);
    tabs::TabInterface* target_tab = tab_list->OpenTab(
        GURL(features::kGlicExperimentalTriggeringTabFocusFallbackURL.Get()),
        tab_list->GetTabCount(), /*foreground=*/false);
    return target_tab ? target_tab->GetContents() : nullptr;
  }

  void SetTickClockForTesting(const base::TickClock* clock) override {
    tick_clock_ = clock;
  }

  views::Widget* GetWidgetForTesting() override { return dialog_widget_.get(); }

  GlicExperimentalOptInDialogView* GetDialogViewForTesting() override {
    return dialog_view_.get();
  }

 private:
  void CloseWidget(views::Widget::ClosedReason reason) {
    tab_subscriptions_.clear();

    if (dialog_widget_) {
      if (!dialog_open_time_.is_null()) {
        base::UmaHistogramMediumTimes(
            "Glic.ExperimentalTriggering.OptInDialog.ShowDuration",
            tick_clock_->NowTicks() - dialog_open_time_);
        dialog_open_time_ = base::TimeTicks();
      }

      if (!visibility_start_time_.is_null()) {
        visible_duration_ += tick_clock_->NowTicks() - visibility_start_time_;
        visibility_start_time_ = base::TimeTicks();
      }

      base::UmaHistogramMediumTimes(
          "Glic.ExperimentalTriggering.OptInDialog.VisibleDuration",
          visible_duration_);
      visible_duration_ = base::TimeDelta();
    }

    bool accepted =
        (reason == views::Widget::ClosedReason::kAcceptButtonClicked);

    dialog_widget_.reset();
    dialog_view_.reset();
    tab_interface_.reset();

    if (delegate_) {
      delegate_->OnUIClosed(accepted);
    }
  }

  void TabDidBecomeVisible(tabs::TabInterface* tab_interface) {
    if (visibility_start_time_.is_null()) {
      visibility_start_time_ = tick_clock_->NowTicks();
    }
  }

  void TabWillBecomeHidden(tabs::TabInterface* tab_interface) {
    if (!visibility_start_time_.is_null()) {
      visible_duration_ += tick_clock_->NowTicks() - visibility_start_time_;
      visibility_start_time_ = base::TimeTicks();
    }
  }

  raw_ptr<Profile> profile_;
  raw_ptr<Delegate> delegate_;
  raw_ptr<const base::TickClock> tick_clock_;
  base::WeakPtr<tabs::TabInterface> tab_interface_;
  std::unique_ptr<GlicExperimentalOptInDialogView> dialog_view_;
  std::unique_ptr<views::Widget> dialog_widget_;

  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  base::TimeTicks dialog_open_time_;
  base::TimeTicks visibility_start_time_;
  base::TimeDelta visible_duration_;

  base::WeakPtrFactory<GlicExperimentalOptInUIHostDesktop> weak_ptr_factory_{
      this};
};

// static
std::unique_ptr<GlicExperimentalOptInUIHost>
GlicExperimentalOptInUIHost::Create(Profile* profile, Delegate* delegate) {
  return std::make_unique<GlicExperimentalOptInUIHostDesktop>(profile,
                                                              delegate);
}

}  // namespace glic
