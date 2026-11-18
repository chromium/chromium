// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "components/actor/core/task_id.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/constants.h"
#endif

namespace {
actor::TaskId GetActorTaskId(content::WebContents& web_contents) {
  if (auto* actor_keyed_service =
          actor::ActorKeyedService::Get(web_contents.GetBrowserContext())) {
    if (auto* task = actor_keyed_service->GetActingActorTaskForWebContents(
            &web_contents)) {
      return task->id();
    }
  }
  return actor::TaskId();
}
}  // namespace

ChromeNavigationUIData::EnvironmentData::EnvironmentData() = default;
ChromeNavigationUIData::EnvironmentData::~EnvironmentData() = default;
ChromeNavigationUIData::EnvironmentData::EnvironmentData(
    const EnvironmentData&) = default;
ChromeNavigationUIData::EnvironmentData&
ChromeNavigationUIData::EnvironmentData::operator=(const EnvironmentData&) =
    default;
ChromeNavigationUIData::EnvironmentData::EnvironmentData(
    EnvironmentData&&) noexcept = default;
ChromeNavigationUIData::EnvironmentData&
ChromeNavigationUIData::EnvironmentData::operator=(EnvironmentData&&) noexcept =
    default;

ChromeNavigationUIData::ChromeNavigationUIData() = default;

ChromeNavigationUIData::ChromeNavigationUIData(const ChromeNavigationUIData&) =
    default;

ChromeNavigationUIData::ChromeNavigationUIData(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle);

  PopulateEnvironmentFromNavigationHandle(*navigation_handle);
}

ChromeNavigationUIData::~ChromeNavigationUIData() = default;

void ChromeNavigationUIData::PopulateEnvironmentFromNavigationHandle(
    content::NavigationHandle& navigation_handle) {
  content::WebContents* web_contents = navigation_handle.GetWebContents();
  CHECK(web_contents);

  EnvironmentData env;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  int tab_id = extension_misc::kUnknownTabId;
  int window_id = extension_misc::kUnknownWindowId;
  // The browser client can be null in unittests.
  if (extensions::ExtensionsBrowserClient::Get()) {
    extensions::ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
        web_contents, &tab_id, &window_id);
  }

  env.extension_data = extensions::ExtensionNavigationUIData(&navigation_handle,
                                                             tab_id, window_id);
#endif

  env.is_no_state_prefetching =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents) != nullptr;
  env.actor_task_id = GetActorTaskId(*web_contents);
  env_ = std::move(env);
}

// static
std::unique_ptr<ChromeNavigationUIData>
ChromeNavigationUIData::CreateForMainFrameNavigation(
    content::WebContents* web_contents,
    bool is_using_https_as_default_scheme,
    bool force_no_https_upgrade) {
  auto navigation_ui_data = std::make_unique<ChromeNavigationUIData>();
  navigation_ui_data->is_using_https_as_default_scheme_ =
      is_using_https_as_default_scheme;
  navigation_ui_data->force_no_https_upgrade_ = force_no_https_upgrade;

  EnvironmentData env;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  int tab_id = extension_misc::kUnknownTabId;
  int window_id = extension_misc::kUnknownWindowId;
  // The browser client can be null in unittests.
  if (extensions::ExtensionsBrowserClient::Get()) {
    extensions::ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
        web_contents, &tab_id, &window_id);
  }

  if (std::unique_ptr<extensions::ExtensionNavigationUIData> extension_data =
          extensions::ExtensionNavigationUIData::CreateForMainFrameNavigation(
              web_contents, tab_id, window_id)) {
    env.extension_data = std::move(*extension_data);
  }
#endif

  env.actor_task_id = GetActorTaskId(*web_contents);
  navigation_ui_data->env_ = std::move(env);

  return navigation_ui_data;
}

// static
std::unique_ptr<ChromeNavigationUIData>
ChromeNavigationUIData::CloneUnboundOrNew(
    const ChromeNavigationUIData* navigation_ui_data) {
  if (!navigation_ui_data) {
    return std::make_unique<ChromeNavigationUIData>();
  }

  CHECK(!navigation_ui_data->env_.has_value());
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  CHECK(!navigation_ui_data->offline_page_data_.is_offline_page());
#endif

  return base::WrapUnique(new ChromeNavigationUIData(*navigation_ui_data));
}

std::unique_ptr<content::NavigationUIData> ChromeNavigationUIData::Clone() {
  return base::WrapUnique(new ChromeNavigationUIData(*this));
}
