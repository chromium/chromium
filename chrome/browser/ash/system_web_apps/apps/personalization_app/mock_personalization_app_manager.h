// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_MOCK_PERSONALIZATION_APP_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_MOCK_PERSONALIZATION_APP_MANAGER_H_

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"

#include "ash/webui/personalization_app/search/search_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::personalization_app {

class MockPersonalizationAppManager : public PersonalizationAppManager {
 public:
  MockPersonalizationAppManager();

  MockPersonalizationAppManager(const MockPersonalizationAppManager&) = delete;
  MockPersonalizationAppManager& operator=(
      const MockPersonalizationAppManager&) = delete;

  ~MockPersonalizationAppManager() override;

  MOCK_METHOD(void,
              MaybeStartHatsTimer,
              (HatsSurveyType hats_survey_type),
              (override));

  MOCK_METHOD((SearchHandler*), search_handler, (), (override));
};

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_MOCK_PERSONALIZATION_APP_MANAGER_H_
