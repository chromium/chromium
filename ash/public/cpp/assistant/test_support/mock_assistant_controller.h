// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_CONTROLLER_H_
#define ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

namespace ash {

class MockAssistantController : public AssistantController {
 public:
  MockAssistantController();
  MockAssistantController(const MockAssistantController&) = delete;
  MockAssistantController& operator=(const MockAssistantController&) = delete;
  ~MockAssistantController() override;

  MOCK_METHOD(void, AddObserver, (AssistantControllerObserver*), (override));

  MOCK_METHOD(void, RemoveObserver, (AssistantControllerObserver*), (override));

  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, bool in_background, bool from_server),
              (override));

  MOCK_METHOD(void, OpenAssistantSettings, (), (override));

  MOCK_METHOD(base::WeakPtr<AssistantController>, GetWeakPtr, (), (override));

  MOCK_METHOD(void,
              SetAssistant,
              (assistant::Assistant * assistant),
              (override));

  MOCK_METHOD(void, StartSpeakerIdEnrollmentFlow, (), (override));

  MOCK_METHOD(void,
              SendAssistantFeedback,
              (bool pii_allowed,
               const std::string& feedback_description,
               const std::string& screenshot_png),
              (override));
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_TEST_SUPPORT_MOCK_ASSISTANT_CONTROLLER_H_
