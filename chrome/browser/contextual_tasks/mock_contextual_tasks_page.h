// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PAGE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PAGE_H_

#include <vector>

#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

class MockContextualTasksPage : public mojom::Page {
 public:
  MockContextualTasksPage();
  ~MockContextualTasksPage() override;

  mojo::PendingRemote<mojom::Page> BindAndGetRemote();

  MOCK_METHOD(void, SetThreadTitle, (const std::string& title), (override));
  MOCK_METHOD(void, SetTaskDetails, (const base::Uuid& uuid), (override));
  MOCK_METHOD(void, SetAimUrl, (const GURL& url), (override));
  MOCK_METHOD(void, OnSidePanelStateChanged, (), (override));
  MOCK_METHOD(void,
              PostMessageToWebview,
              (const std::vector<uint8_t>& message),
              (override));
  MOCK_METHOD(void, OnHandshakeComplete, (), (override));
  MOCK_METHOD(void,
              SetOAuthToken,
              (const std::string& oauth_token),
              (override));
  MOCK_METHOD(void,
              OnContextUpdated,
              (std::vector<mojom::ContextInfoPtr> context),
              (override));
  MOCK_METHOD(void, HideInput, (), (override));
  MOCK_METHOD(void, RestoreInput, (), (override));
  MOCK_METHOD(void, EnterBasicMode, (), (override));
  MOCK_METHOD(void, ExitBasicMode, (), (override));
  MOCK_METHOD(void, OnZeroStateChange, (bool is_zero_state), (override));
  MOCK_METHOD(void, OnAiPageStatusChanged, (bool is_ai_page), (override));
  MOCK_METHOD(void,
              OnLensOverlayStateChanged,
              (bool is_showing, bool maybe_show_overlay_hint_text),
              (override));
  MOCK_METHOD(void, ShowErrorPage, (), (override));
  MOCK_METHOD(void, HideErrorPage, (), (override));
  MOCK_METHOD(void, ShowOauthErrorDialog, (), (override));
  MOCK_METHOD(void,
              UpdateComposeboxPosition,
              (mojom::ComposeboxPositionPtr position),
              (override));
  MOCK_METHOD(void, LockInput, (), (override));
  MOCK_METHOD(void, UnlockInput, (), (override));
  MOCK_METHOD(void, SetShowReopenTabs, (bool show), (override));
  MOCK_METHOD(void, SetExpandButtonEnabled, (bool enabled), (override));
  MOCK_METHOD(void, TurnOnSmartTabSharing, (), (override));
  MOCK_METHOD(void, InjectInput, (mojom::InjectedInputPtr input), (override));
  MOCK_METHOD(void,
              RemoveInjectedInput,
              (const base::UnguessableToken& file_token),
              (override));
  MOCK_METHOD(void, OnSidePanelPinStateChanged, (bool is_pinned), (override));
  MOCK_METHOD(void, SetInNlm, (bool in_nlm), (override));

 private:
  mojo::Receiver<mojom::Page> receiver_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PAGE_H_
