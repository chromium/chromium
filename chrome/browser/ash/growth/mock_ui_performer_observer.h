// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GROWTH_MOCK_UI_PERFORMER_OBSERVER_H_
#define CHROME_BROWSER_ASH_GROWTH_MOCK_UI_PERFORMER_OBSERVER_H_

#include "chrome/browser/ash/growth/ui_action_performer.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockUiPerformerObserver : public UiActionPerformer::Observer {
 public:
  MockUiPerformerObserver();
  ~MockUiPerformerObserver() override;

  // UiActionPerformer::Observer:
  MOCK_METHOD(void, OnReadyToLogImpression, (int), (override));

  MOCK_METHOD(void, OnDismissed, (int, bool), (override));

  MOCK_METHOD(void, OnButtonPressed, (int, CampaignButtonId, bool), (override));
};

#endif  // CHROME_BROWSER_ASH_GROWTH_MOCK_UI_PERFORMER_OBSERVER_H_
