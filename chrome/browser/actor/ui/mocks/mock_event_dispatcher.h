// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_EVENT_DISPATCHER_H_

#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor {
class ToolRequest;
namespace ui {

class MockUiEventDispatcher : public UiEventDispatcher {
 public:
  MockUiEventDispatcher();
  ~MockUiEventDispatcher() override;
  MOCK_METHOD(void,
              OnPreTool,
              (const ToolRequest& tool_request, UiCompleteCallback callback),
              (override));

  MOCK_METHOD(void,
              OnPostTool,
              (const ToolRequest& tool_request, UiCompleteCallback callback),
              (override));

  MOCK_METHOD(void,
              OnActorTaskAsyncChange,
              (const ActorTaskAsyncChange& change, UiCompleteCallback callback),
              (override));

  MOCK_METHOD(void,
              OnActorTaskSyncChange,
              (const ActorTaskSyncChange& change),
              (override));
};

std::unique_ptr<UiEventDispatcher> NewMockUiEventDispatcher();

}  // namespace ui
}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_EVENT_DISPATCHER_H_
