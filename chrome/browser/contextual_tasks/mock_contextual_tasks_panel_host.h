// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PANEL_HOST_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PANEL_HOST_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

class MockContextualTasksPanelHost : public ContextualTasksPanelHost {
 public:
  MockContextualTasksPanelHost();
  ~MockContextualTasksPanelHost() override;

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(void, Show, (AnimationStyle animation), (override));
  MOCK_METHOD(void, Close, (AnimationStyle animation), (override));
  MOCK_METHOD(bool, IsPanelInitialized, (), (override));
  MOCK_METHOD(bool, IsPanelOpenForContextualTask, (), (const override));
  MOCK_METHOD(bool, IsPanelSuppressed, (), (const override));
  MOCK_METHOD(void,
              SetPanelSuppressedForTesting,
              (bool suppressed),
              (override));
  content::WebContents* GetWebContents() override {
    return web_contents_.get();
  }
  void SetWebContents(content::WebContents* web_contents) override {
    web_contents_ = web_contents ? web_contents->GetWeakPtr() : nullptr;
  }

 private:
  base::WeakPtr<content::WebContents> web_contents_ = nullptr;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_PANEL_HOST_H_
