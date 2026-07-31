// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/setters/default_browser_visual_guided_setter.h"

#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/types/to_address.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_features.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace default_browser {

class DefaultBrowserVisualGuidedSetter::ExecutionRunner
    : public content::WebContentsObserver {
 public:
  ExecutionRunner(Profile& profile,
                  DefaultBrowserSetterCompletionCallback on_complete)
      : profile_(profile), on_complete_(std::move(on_complete)) {}

  ~ExecutionRunner() override {
    if (on_complete_) {
      std::move(on_complete_).Run(DefaultBrowserState::NOT_DEFAULT);
    }
  }

  void Run(const ExecuteParams& params) {
    GURL url = GetDefaultBrowserVisualGuideURL();

    if (params.can_pin_to_taskbar) {
      url = net::AppendQueryParameter(url, "can_pin_to_taskbar", "true");
    }

    NavigateParams navigation_params(base::to_address(profile_), url,
                                     ui::PAGE_TRANSITION_LINK);
    navigation_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    navigation_params.window_action = NavigateParams::WindowAction::kShowWindow;
    Navigate(&navigation_params);

    if (!navigation_params.navigated_or_inserted_contents) {
      Finish(DefaultBrowserState::NOT_DEFAULT);
      return;
    }

    Observe(navigation_params.navigated_or_inserted_contents);

    if (auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents())) {
      tab_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
          &ExecutionRunner::OnTabWillDetach, weak_ptr_factory_.GetWeakPtr()));
    }

    if (auto* manager = DefaultBrowserManager::From(g_browser_process)) {
      manager_subscription_ = manager->RegisterDefaultBrowserChanged(
          base::BindRepeating(&ExecutionRunner::OnDefaultBrowserChanged,
                              weak_ptr_factory_.GetWeakPtr()));
      manager->GetDefaultBrowserState(
          base::BindOnce(&ExecutionRunner::OnDefaultBrowserChanged,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override {
    if (web_contents() && (!web_contents()->GetLastCommittedURL().SchemeIs(
                               content::kChromeUIScheme) ||
                           web_contents()->GetLastCommittedURL().host() !=
                               GetDefaultBrowserVisualGuideURL().host())) {
      Finish(DefaultBrowserState::NOT_DEFAULT);
    }
  }

 private:
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason) {
    if (reason == tabs::TabInterface::DetachReason::kDelete) {
      Finish(DefaultBrowserState::NOT_DEFAULT);
    } else if (reason ==
               tabs::TabInterface::DetachReason::kInsertIntoOtherWindow) {
      tab_detach_subscription_ = tab->RegisterDidInsert(base::BindRepeating(
          &ExecutionRunner::OnTabInserted, weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void OnTabInserted(tabs::TabInterface* tab) {
    CHECK(tab);
    tab_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
        &ExecutionRunner::OnTabWillDetach, weak_ptr_factory_.GetWeakPtr()));
  }

  void OnDefaultBrowserChanged(DefaultBrowserState state) {
    if (state != DefaultBrowserState::IS_DEFAULT) {
      return;
    }
    content::WebContents* contents = web_contents();
    Observe(nullptr);
    tab_detach_subscription_ = {};
    if (contents) {
      NavigateParams params(base::to_address(profile_),
                            GURL(chrome::kChromeUINewTabURL),
                            ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
      params.source_contents = contents;
      params.disposition = WindowOpenDisposition::CURRENT_TAB;
      Navigate(&params);
    }
    Finish(DefaultBrowserState::IS_DEFAULT);
  }

  void Finish(DefaultBrowserState state) {
    Observe(nullptr);
    tab_detach_subscription_ = {};
    manager_subscription_ = {};
    if (on_complete_) {
      std::move(on_complete_).Run(state);
    }
  }

  const raw_ref<Profile> profile_;
  DefaultBrowserSetterCompletionCallback on_complete_;
  base::CallbackListSubscription tab_detach_subscription_;
  base::CallbackListSubscription manager_subscription_;
  base::WeakPtrFactory<ExecutionRunner> weak_ptr_factory_{this};
};

DefaultBrowserVisualGuidedSetter::DefaultBrowserVisualGuidedSetter(
    Profile& profile)
    : profile_(profile) {}

DefaultBrowserVisualGuidedSetter::~DefaultBrowserVisualGuidedSetter() = default;

DefaultBrowserSetterType DefaultBrowserVisualGuidedSetter::GetType() const {
  return DefaultBrowserSetterType::kVisualGuide;
}

void DefaultBrowserVisualGuidedSetter::Execute(
    DefaultBrowserSetterCompletionCallback on_complete,
    const ExecuteParams& params) {
  runner_ =
      std::make_unique<ExecutionRunner>(*profile_, std::move(on_complete));
  runner_->Run(params);
}

}  // namespace default_browser
