// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <utility>
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
#include "chrome/browser/glic/browser_ui/glic_modal.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui_host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace glic {

namespace {

constexpr int kMinDialogHeight = 200;
constexpr int kMaxDialogHeight = 800;

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
      : profile_(profile), delegate_(delegate) {}

  ~GlicExperimentalOptInUIHostDesktop() override {
    delegate_ = nullptr;
    Close(/*accepted=*/false);
  }

  void Show(content::WebContents* web_contents) override {
    if (modal_) {
      modal_->Activate();
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

    RequiredExperimentalOptIn required_state =
        RequiredExperimentalOptIn::kExperimental;
    if (auto* service =
            GlicKeyedServiceFactory::GetGlicKeyedService(profile_)) {
      required_state = service->enabling().GetRequiredExperimentalOptIn();
    }
    if (required_state == RequiredExperimentalOptIn::kNotNeeded) {
      required_state = RequiredExperimentalOptIn::kExperimental;
    }

    GlicModal::Params params;
    params.content_url = GURL(chrome::kChromeUIGlicExperimentalOptInURL);
    params.initial_size =
        gfx::Size(kGlicExperimentalOptInDefaultWidth,
                  (required_state == RequiredExperimentalOptIn::kExperimental)
                      ? kGlicExperimentalOptInDefaultHeightExperimental
                      : kGlicExperimentalOptInDefaultHeightGlic);
    params.min_size =
        gfx::Size(kGlicExperimentalOptInDefaultWidth, kMinDialogHeight);
    params.max_size =
        gfx::Size(kGlicExperimentalOptInDefaultWidth, kMaxDialogHeight);
    params.size_to_content = true;
    params.suppress_first_auto_resize = true;
    const bool a11y_fix =
        base::FeatureList::IsEnabled(features::kGlicOptInDialogA11yFix);
    params.focus_web_view_on_open = a11y_fix;
    if (a11y_fix) {
      params.accessible_title =
          l10n_util::GetStringUTF16(IDS_GLIC_EXPERIMENTAL_OPT_IN_TITLE);
    }
    params.on_closed =
        base::BindOnce(&GlicExperimentalOptInUIHostDesktop::OnModalClosed,
                       weak_ptr_factory_.GetWeakPtr());

    is_accepted_ = false;
    modal_ = GlicModal::Show(*tab_interface, std::move(params));
    if (!modal_) {
      tab_interface_.reset();
      if (delegate_) {
        delegate_->OnUIClosed(/*accepted=*/false);
      }
      return;
    }

    OnDialogOpened();
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
  }

  void Close(bool accepted) override {
    is_accepted_ = accepted;
    if (modal_) {
      modal_->Close();
    } else {
      OnModalClosed();
    }
  }

  void OpenLinkInNewTab(const GURL& url) override {
    if (!tab_interface_) {
      return;
    }
    if (auto* browser_window = tab_interface_->GetBrowserWindowInterface()) {
      browser_window->OpenGURL(
          url, ui::mojom::WindowOpenDisposition::kNewForegroundTab);
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

  views::Widget* GetWidgetForTesting() override {
    tabs::TabInterface* tab = tab_interface_.get();
    tabs::TabDialogManager* manager =
        tab ? tabs::TabDialogManager::From(tab) : nullptr;
    // Ensure this host actually owns an active modal before querying the
    // tab's shared TabDialogManager, which could otherwise return a widget
    // belonging to a different tab-modal dialog on `tab`.
    if (!modal_ || !manager) {
      return nullptr;
    }
    views::Widget* host_widget = manager->GetHostWidget();
    if (!host_widget) {
      return nullptr;
    }
    views::Widget::Widgets owned_widgets =
        views::Widget::GetAllOwnedWidgets(host_widget->GetNativeView());
    for (views::Widget* widget : owned_widgets) {
      if (manager->IsDialogManaged(widget)) {
        return widget;
      }
    }
    return nullptr;
  }

  views::WebView* GetWebViewForTesting() override {
    views::Widget* widget = GetWidgetForTesting();  // IN-TEST
    return widget && widget->GetRootView()
               ? views::AsViewClass<views::WebView>(
                     widget->GetRootView()->GetViewByElementId(
                         GlicModal::kDialogElementId))
               : nullptr;
  }

 private:
  void OnModalClosed() {
    tab_subscriptions_.clear();

    if (modal_) {
      RecordShowDurationOnClose();

      if (!visibility_start_time_.is_null()) {
        visible_duration_ += tick_clock_->NowTicks() - visibility_start_time_;
        visibility_start_time_ = base::TimeTicks();
      }

      base::UmaHistogramMediumTimes(
          "Glic.ExperimentalTriggering.OptInDialog.VisibleDuration",
          visible_duration_);
      visible_duration_ = base::TimeDelta();
    }

    const bool accepted = std::exchange(is_accepted_, false);

    modal_.reset();
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
  base::WeakPtr<tabs::TabInterface> tab_interface_;
  std::unique_ptr<GlicModal> modal_;
  bool is_accepted_ = false;

  std::vector<base::CallbackListSubscription> tab_subscriptions_;
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
