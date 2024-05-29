// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENDED_UPDATES_TEST_MOCK_EXTENDED_UPDATES_CONTROLLER_H_
#define CHROME_BROWSER_ASH_EXTENDED_UPDATES_TEST_MOCK_EXTENDED_UPDATES_CONTROLLER_H_

#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockExtendedUpdatesController : public ExtendedUpdatesController {
 public:
  MockExtendedUpdatesController();
  MockExtendedUpdatesController(const MockExtendedUpdatesController&) = delete;
  MockExtendedUpdatesController& operator=(
      const MockExtendedUpdatesController&) = delete;
  ~MockExtendedUpdatesController() override;

  MOCK_METHOD(bool,
              IsOptInEligible,
              (content::BrowserContext*, const Params&),
              (override));

  MOCK_METHOD(bool, IsOptInEligible, (content::BrowserContext*), (override));

  MOCK_METHOD(bool, IsOptedIn, (), (override));

  MOCK_METHOD(void,
              OnEolInfo,
              (content::BrowserContext*, const UpdateEngineClient::EolInfo&),
              (override));

  MOCK_METHOD(bool,
              HasOptInAbility,
              (ownership::OwnerSettingsService*),
              (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EXTENDED_UPDATES_TEST_MOCK_EXTENDED_UPDATES_CONTROLLER_H_
