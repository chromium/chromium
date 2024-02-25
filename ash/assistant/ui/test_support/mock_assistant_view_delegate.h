// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_TEST_SUPPORT_MOCK_ASSISTANT_VIEW_DELEGATE_H_
#define ASH_ASSISTANT_UI_TEST_SUPPORT_MOCK_ASSISTANT_VIEW_DELEGATE_H_

#include <cstdint>
#include <map>
#include <string>

#include "ash/assistant/ui/assistant_view_delegate.h"
#include "base/component_export.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace ash {

// A mock implementation of AssistantViewDelegate for use in tests.
class COMPONENT_EXPORT(ASSISTANT_UI) MockAssistantViewDelegate
    : public testing::NiceMock<AssistantViewDelegate> {
 public:
  MockAssistantViewDelegate();
  MockAssistantViewDelegate(const MockAssistantViewDelegate&) = delete;
  MockAssistantViewDelegate& operator=(const MockAssistantViewDelegate&) =
      delete;
  ~MockAssistantViewDelegate() override;

  MOCK_METHOD((const AssistantNotificationModel*),
              GetNotificationModel,
              (),
              (const, override));

  MOCK_METHOD(void, AddObserver, (AssistantViewDelegateObserver*), (override));

  MOCK_METHOD(void,
              RemoveObserver,
              (AssistantViewDelegateObserver*),
              (override));

  MOCK_METHOD(void,
              DownloadImage,
              (const GURL&, ImageDownloader::DownloadCallback),
              (override));

  MOCK_METHOD(::wm::CursorManager*, GetCursorManager, (), (override));

  MOCK_METHOD(std::string, GetPrimaryUserGivenName, (), (const, override));

  MOCK_METHOD(aura::Window*, GetRootWindowForDisplayId, (int64_t), (override));

  MOCK_METHOD(aura::Window*, GetRootWindowForNewWindows, (), (override));

  MOCK_METHOD(bool, IsTabletMode, (), (const, override));

  MOCK_METHOD(void,
              OnDialogPlateButtonPressed,
              (AssistantButtonId),
              (override));

  MOCK_METHOD(void,
              OnDialogPlateContentsCommitted,
              (const std::string&),
              (override));

  MOCK_METHOD(void,
              OnNotificationButtonPressed,
              (const std::string&, int),
              (override));

  MOCK_METHOD(void, OnOnboardingShown, (), (override));

  MOCK_METHOD(void, OnOptInButtonPressed, (), (override));

  MOCK_METHOD(void,
              OnSuggestionPressed,
              (const base::UnguessableToken& suggestion_id),
              (override));

  MOCK_METHOD(bool, ShouldShowOnboarding, (), (const, override));

  MOCK_METHOD(void,
              OnLauncherSearchChipPressed,
              (const std::u16string&),
              (override));
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_TEST_SUPPORT_MOCK_ASSISTANT_VIEW_DELEGATE_H_
