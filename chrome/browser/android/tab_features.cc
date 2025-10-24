// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_features.h"

#include "chrome/browser/net/qwac_web_contents_observer.h"
#include "chrome/browser/preloading/new_tab_page_preload/new_tab_page_preload_pipeline_manager.h"
#include "chrome/browser/sync/sessions/sync_sessions_router_tab_helper.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/lens/tab_contextualization_controller.h"
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
  tab_contextualization_controller_ =
      GetUserDataFactory().CreateInstance<lens::TabContextualizationController>(
          *tab, tab);
}

TabFeatures::~TabFeatures() = default;

// static
ui::UserDataFactoryWithOwner<TabInterface>& TabFeatures::GetUserDataFactory() {
  static base::NoDestructor<ui::UserDataFactoryWithOwner<TabInterface>> factory;
  return *factory;
}

}  // namespace tabs
