// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_H_

#include <memory>

#include "ash/webui/personalization_app/search/search_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace local_search_service {
class LocalSearchServiceProxy;
}

namespace personalization_app {

// Manager for the Chrome OS Personalization App. This class is implemented as a
// KeyedService, so one instance of the class is intended to be active for the
// lifetime of a logged-in user, even if the personalization app is not opened.
class PersonalizationAppManager : public KeyedService {
 public:
  static std::unique_ptr<PersonalizationAppManager> Create(
      content::BrowserContext* context,
      local_search_service::LocalSearchServiceProxy&
          local_search_service_proxy);

  ~PersonalizationAppManager() override = default;

  virtual SearchHandler* search_handler() = 0;
};

}  // namespace personalization_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_MANAGER_H_
