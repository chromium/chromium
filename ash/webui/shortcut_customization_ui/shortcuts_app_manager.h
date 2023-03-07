// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUTS_APP_MANAGER_H_
#define ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUTS_APP_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"

#include <memory>

namespace ash {

namespace local_search_service {
class LocalSearchServiceProxy;
}

namespace shortcut_ui {

class SearchHandler;
class SearchConceptRegistry;

// Manager for the ChromeOS Shortcuts app. This class is implemented as a
// KeyedService, so one instance of the class is intended to be active for the
// lifetime of a logged-in user, even if the Shortcuts app is not opened.
//
// The main responsibility of this manager class is to support search queries
// for shortcuts. It's responsible for initializing and managing the various
// search-related classes like SearchHandler and SearchConceptRegistry, and it's
// also responsible for retrieving the list of accelerators and passing them
// to the SearchConceptRegistry so that they can be added to the Local Search
// Service index.
class ShortcutsAppManager : public KeyedService {
 public:
  explicit ShortcutsAppManager(local_search_service::LocalSearchServiceProxy*
                                   local_search_service_proxy);
  ShortcutsAppManager(const ShortcutsAppManager& other) = delete;
  ShortcutsAppManager& operator=(const ShortcutsAppManager& other) = delete;
  ~ShortcutsAppManager() override;

  SearchHandler* search_handler() { return search_handler_.get(); }

 private:
  // KeyedService:
  void Shutdown() override;

  std::unique_ptr<SearchConceptRegistry> search_concept_registry_;
  std::unique_ptr<SearchHandler> search_handler_;
};

}  // namespace shortcut_ui
}  // namespace ash

#endif  // ASH_WEBUI_SHORTCUT_CUSTOMIZATION_UI_SHORTCUTS_APP_MANAGER_H_