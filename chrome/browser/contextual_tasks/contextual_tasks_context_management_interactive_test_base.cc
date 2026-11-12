// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_management_interactive_test_base.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/timer/timer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
// TabUnderlineView currently resides in glic/browser_ui.
#include "chrome/browser/glic/browser_ui/tab_underline_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/url_loader_interceptor.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/views/view_utils.h"

namespace contextual_tasks {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kSidePanelWebContentsId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kInnerWebContentsId);

DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kElementExistsEvent);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kMenuOpenEvent);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kFlyoutVisibleEvent);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kCoinsReadyEvent);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kTriggerReadyEvent);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kCheckedReadyEvent);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kSubmitEnabledEvent);
DEFINE_CUSTOM_ELEMENT_EVENT_TYPE(kUploadsCompleteEvent);

ContextualTasksContextManagementInteractiveTestBase::
    ContextualTasksContextManagementInteractiveTestBase(
        DeepQuery composebox_root)
    : kComposeboxContainer(std::move(composebox_root)),
      kContextEntrypoint(kComposeboxContainer + "#contextEntrypoint" +
                         "#entrypointButton" + "#entrypoint"),
      kContextMenu(kComposeboxContainer + "#contextEntrypoint" + "#menu"),
      kShareTabsTrigger(kContextMenu + "#shareTabsTrigger"),
      kShareTabsFlyout(kContextMenu + ".share-tabs-flyout") {}

ContextualTasksContextManagementInteractiveTestBase::
    ~ContextualTasksContextManagementInteractiveTestBase() = default;

void ContextualTasksContextManagementInteractiveTestBase::SetUpFeatureList() {
  std::vector<base::test::FeatureRefAndParams> enabled_features =
      GetDefaultEnabledFeatures();
  enabled_features.push_back({kContextualTasksForceEntryPointEligibility, {}});
  enabled_features.push_back({omnibox::kContextManagementInComposebox,
                              {{"enable_tab_deselection", "true"}}});
  enabled_features.push_back({omnibox::kTabFaviconChipsToCoins, {}});
  std::vector<base::test::FeatureRef> disabled_features =
      GetDefaultDisabledFeatures();
  // These tests expect New thread to start the new thread in the panel shared
  // by the tabs in a task.
  disabled_features.push_back(kContextualTasksSidePanelTabState);
  feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                              disabled_features);
}

void ContextualTasksContextManagementInteractiveTestBase::SetUpOnMainThread() {
  ContextualTasksInteractiveTestBase::SetUpOnMainThread();
  SidePanelUI::From(browser())->DisableAnimationsForTesting();

  url_loader_interceptor_.reset();
  url_loader_interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
      base::BindLambdaForTesting(
          [this](content::URLLoaderInterceptor::RequestParams* params) {
            const GURL& url = params->url_request.url;
            if (url.host().find("lensfrontend-pa") != std::string::npos) {
              if (url.path().find("gsessionid") != std::string::npos) {
                lens::LensOverlayServerClusterInfoResponse cluster_info;
                cluster_info.set_server_session_id("fake_server_session_id");
                cluster_info.set_search_session_id("fake_search_session_id");
                std::string response_data;
                cluster_info.SerializeToString(&response_data);
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\r\nContent-Type: "
                    "application/x-protobuf\r\n\r\n",
                    response_data, params->client.get());
                return true;
              }
              if (fail_context_uploads_) {
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 500 Internal Server Error\r\n\r\n", "",
                    params->client.get());
                return true;
              }
              // Keep the context upload in flight by retaining the client
              // without ever responding. Dropping it would complete the
              // request with an error instead.
              if (ShouldStallContextUploads()) {
                stalled_upload_clients_.push_back(std::move(params->client));
                return true;
              }
              lens::LensOverlayServerResponse upload_response;
              std::string response_data;
              upload_response.SerializeToString(&response_data);
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\r\nContent-Type: "
                  "application/x-protobuf\r\n\r\n",
                  response_data, params->client.get());
              return true;
            }
            if (url.host() == "www.google.com" ||
                (url.host() == "127.0.0.1" && url.path() == "/search")) {
              content::URLLoaderInterceptor::WriteResponse(
                  "chrome/test/data/mock_aim_page.html", params->client.get());
              return true;
            }
            return false;
          }));
}

void ContextualTasksContextManagementInteractiveTestBase::
    TearDownOnMainThread() {
  url_loader_interceptor_.reset();
  ContextualTasksInteractiveTestBase::TearDownOnMainThread();
}

bool ContextualTasksContextManagementInteractiveTestBase::RunUntilNestable(
    base::FunctionRef<bool()> condition) {
  if (condition()) {
    return true;
  }
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, base::Milliseconds(10),
              base::BindLambdaForTesting([&condition, &timer, &run_loop]() {
                if (condition()) {
                  timer.Stop();
                  run_loop.Quit();
                }
              }));
  run_loop.Run();
  return condition();
}

base::Uuid ContextualTasksContextManagementInteractiveTestBase::TaskIdForTab(
    int index) {
  content::WebContents* contents =
      browser()->GetAllTabInterfaces()[index]->GetContents();
  std::optional<ContextualTask> task =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile())
          ->GetContextualTaskForTab(
              sessions::SessionTabHelper::IdForTab(contents));
  return task ? task->GetTaskId() : base::Uuid();
}

bool ContextualTasksContextManagementInteractiveTestBase::
    ShouldStallContextUploads() const {
  return false;
}

void ContextualTasksContextManagementInteractiveTestBase::FailStalledUploads() {
  EXPECT_TRUE(
      RunUntilNestable([this]() { return !stalled_upload_clients_.empty(); }));
  fail_context_uploads_ = true;
  for (auto& client : stalled_upload_clients_) {
    content::URLLoaderInterceptor::WriteResponse(
        "HTTP/1.1 500 Internal Server Error\r\n\r\n", "", client.get());
  }
  stalled_upload_clients_.clear();
}

bool ContextualTasksContextManagementInteractiveTestBase::
    WaitForUnderlinedTabsToMatch(const std::set<int>& expected_indices) {
  return RunUntilNestable([this, &expected_indices]() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    if (!browser_view || !browser_view->tab_strip_view()) {
      return false;
    }
    auto* tab_list = TabListInterface::From(browser());
    if (!tab_list) {
      return false;
    }
    for (int i = 0; i < tab_list->GetTabCount(); ++i) {
      tabs::TabInterface* tab = tab_list->GetTab(i);
      if (!tab) {
        return false;
      }
      views::View* tab_anchor =
          browser_view->tab_strip_view()->GetTabAnchorView(tab->GetHandle());
      if (!tab_anchor) {
        return false;
      }
      auto* underline = views::AsViewClass<glic::TabUnderlineView>(
          tab_anchor->GetViewByElementId(
              glic::TabUnderlineView::kGlicTabUnderlineElementId));
      bool is_showing = underline && underline->IsShowing();
      bool should_underline = expected_indices.contains(i);
      if (is_showing != should_underline) {
        return false;
      }
    }
    return true;
  });
}

void ContextualTasksContextManagementInteractiveTestBase::
    ShowSidePanelForTesting() {
  if (auto* lens_controller = LensSearchController::FromTabWebContents(
          browser()->GetActiveTabInterface()->GetContents())) {
    lens_controller->SetInvocationSource(
        lens::LensOverlayInvocationSource::kOmniboxPageAction);
  }
  auto* coordinator = ContextualTasksPanelController::From(browser());
  coordinator->Show(
      false, omnibox::DESKTOP_CHROME_LENS_CONTEXTUAL_SEARCHBOX_ENTRY_POINT);
}

}  // namespace contextual_tasks
