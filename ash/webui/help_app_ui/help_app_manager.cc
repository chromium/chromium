// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/help_app_manager.h"

#include "ash/webui/help_app_ui/search/search_handler.h"
#include "ash/webui/help_app_ui/search/search_tag_registry.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {
namespace help_app {

HelpAppManager::HelpAppManager(
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy)
    : search_tag_registry_(
          std::make_unique<SearchTagRegistry>(local_search_service_proxy)),
      search_handler_(
          std::make_unique<SearchHandler>(search_tag_registry_.get(),
                                          local_search_service_proxy)) {}

HelpAppManager::~HelpAppManager() = default;

void HelpAppManager::Shutdown() {
  // Note: These must be deleted in the opposite order of their creation to
  // prevent against UAF violations.
  search_handler_.reset();
  search_tag_registry_.reset();
}

}  // namespace help_app
}  // namespace ash
