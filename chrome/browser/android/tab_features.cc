// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_features.h"

#include <memory>

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_tab_data.h"
#include "chrome/browser/actor/android/ui/actor_ui_tab_controller_android.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_desktop_android.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ask_before_http_dialog_controller.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/side_panel/android/android_side_panel_enabled_fn.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/side_panel_container/internal/android/dev/side_panel_tab_scoped_dev_feature.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "components/actor/core/actor_features.h"
#include "components/contextual_tasks/public/features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/security_interstitials/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "net/base/features.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

namespace tabs {

TabFeatures::TabFeatures(content::WebContents* web_contents, Profile* profile) {
  sync_sessions_router_ =
      std::make_unique<sync_sessions::SyncSessionsRouterTabHelper>(
          web_contents,
          sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
              profile),
          ChromeTranslateClient::FromWebContents(web_contents),
          favicon::ContentFaviconDriver::FromWebContents(web_contents));

  if (base::FeatureList::IsEnabled(net::features::kVerifyQWACs)) {
    qwac_web_contents_observer_ =
        std::make_unique<QwacWebContentsObserver>(web_contents);
  }

  new_tab_page_preload_pipeline_manager_ =
      std::make_unique<NewTabPagePreloadPipelineManager>(web_contents);

  TabInterface* const tab = TabInterface::GetFromContents(web_contents);

  if (base::FeatureList::IsEnabled(
          security_interstitials::features::kHttpsFirstDialogUi)) {
    ask_before_http_dialog_controller_ =
        GetUserDataFactory().CreateInstance<AskBeforeHttpDialogController>(*tab,
                                                                           tab);
  }

  tab_scoped_side_panel_registry_ =
      AndroidSidePanelEnabledFn::IsEnabled()
          ? std::make_unique<SidePanelRegistry>(tab)
          : nullptr;

  if (tab_scoped_side_panel_registry_ &&
      base::FeatureList::IsEnabled(
          chrome::android::kEnableAndroidSidePanelDevFeature)) {
    std::string scope = base::GetFieldTrialParamValueByFeature(
        chrome::android::kEnableAndroidSidePanelDevFeature, "scope");
    if (scope == "tab") {
      tab_scoped_side_panel_dev_feature_ =
          std::make_unique<SidePanelTabScopedDevFeature>(
              tab, tab_scoped_side_panel_registry_.get());
    }
  }

  if (base::FeatureList::IsEnabled(features::kGlicActor)) {
    actor_tab_data_ =
        GetUserDataFactory().CreateInstance<actor::ActorTabData>(*tab, tab);
  }

  auto* actor_service = actor::ActorKeyedService::Get(profile);
  if (glic::GlicEnabling::IsProfileEligible(profile) && actor_service) {
    actor_ui_tab_controller_ =
        GetUserDataFactory()
            .CreateInstance<actor::ui::ActorUiTabControllerAndroid>(
                *tab, *tab, actor_service);
  }

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasksContext)) {
    contextual_tasks_tab_visit_tracker_ =
        GetUserDataFactory()
            .CreateInstance<contextual_tasks::ContextualTasksTabVisitTracker>(
                *tab, *tab);
  }

  tab_contextualization_controller_ =
      GetUserDataFactory().CreateInstance<lens::TabContextualizationController>(
          *tab, tab);

  glic_instance_helper_ =
      GetUserDataFactory().CreateInstance<glic::GlicInstanceHelper>(*tab, tab);
  if (base::FeatureList::IsEnabled(features::kGlicAndroidSidePanel)) {
    glic_side_panel_coordinator_ =
        GetUserDataFactory()
            .CreateInstance<glic::GlicSidePanelCoordinatorDesktopAndroid>(
                *tab, tab, tab_scoped_side_panel_registry_.get(), profile);
  } else {
    glic_side_panel_coordinator_ =
        GetUserDataFactory()
            .CreateInstance<glic::GlicSidePanelCoordinatorAndroid>(*tab, tab);
  }
}

TabFeatures::~TabFeatures() = default;

// static
ui::UserDataFactoryWithOwner<TabInterface>& TabFeatures::GetUserDataFactory() {
  static base::NoDestructor<ui::UserDataFactoryWithOwner<TabInterface>> factory;
  return *factory;
}

}  // namespace tabs
