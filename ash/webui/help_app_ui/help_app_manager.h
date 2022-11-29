// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_HELP_APP_MANAGER_H_
#define ASH_WEBUI_HELP_APP_UI_HELP_APP_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"

#include <memory>

namespace ash {

namespace local_search_service {
class LocalSearchServiceProxy;
}

namespace help_app {

class SearchHandler;
class SearchTagRegistry;

// Manager for the Chrome OS help app. This class is implemented as a
// KeyedService, so one instance of the class is intended to be active for the
// lifetime of a logged-in user, even if the help app is not opened.
//
// Main responsibilities:
//
// (1) Support search queries for help content. HelpAppManager is
//     responsible for updating the kHelpAppLauncher index of the
//     LocalSearchService with search tags corresponding to the top help
//     articles.
class HelpAppManager : public KeyedService {
 public:
  HelpAppManager(local_search_service::LocalSearchServiceProxy*
                     local_search_service_proxy);
  HelpAppManager(const HelpAppManager& other) = delete;
  HelpAppManager& operator=(const HelpAppManager& other) = delete;
  ~HelpAppManager() override;

  SearchHandler* search_handler() { return search_handler_.get(); }

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<SearchTagRegistry> search_tag_registry_;
  std::unique_ptr<SearchHandler> search_handler_;
};

}  // namespace help_app
}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_MANAGER_H_
