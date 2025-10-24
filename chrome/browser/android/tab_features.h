// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
#define CHROME_BROWSER_ANDROID_TAB_FEATURES_H_

#include <memory>

#include "ui/base/unowned_user_data/user_data_factory.h"

class Profile;
class QwacWebContentsObserver;
class NewTabPagePreloadPipelineManager;

namespace content {
class WebContents;
}  // namespace content

namespace sync_sessions {
class SyncSessionsRouterTabHelper;
}  // namespace sync_sessions

namespace lens {
class TabContextualizationController;
}  // namespace lens

namespace tabs {

class TabInterface;

// This class holds state that is scoped to a tab in Android. It is constructed
// after the WebContents/tab_helpers, and destroyed before.
class TabFeatures {
 public:
  TabFeatures(content::WebContents* web_contents, Profile* profile);
  ~TabFeatures();

  NewTabPagePreloadPipelineManager* new_tab_page_preload_pipeline_manager() {
    return new_tab_page_preload_pipeline_manager_.get();
  }

  lens::TabContextualizationController* tab_contextualization_controller() {
    return tab_contextualization_controller_.get();
  }

 private:
  // Returns the factory used to create owned components.
  static ui::UserDataFactoryWithOwner<TabInterface>& GetUserDataFactory();

  std::unique_ptr<sync_sessions::SyncSessionsRouterTabHelper>
      sync_sessions_router_;
  std::unique_ptr<QwacWebContentsObserver> qwac_web_contents_observer_;
  std::unique_ptr<NewTabPagePreloadPipelineManager>
      new_tab_page_preload_pipeline_manager_;
  std::unique_ptr<lens::TabContextualizationController>
      tab_contextualization_controller_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_FEATURES_H_
