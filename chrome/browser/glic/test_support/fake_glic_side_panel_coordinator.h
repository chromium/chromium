// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_GLIC_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_GLIC_SIDE_PANEL_COORDINATOR_H_

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"

namespace glic {

class FakeGlicSidePanelCoordinator : public GlicSidePanelCoordinator {
 public:
  explicit FakeGlicSidePanelCoordinator(tabs::TabInterface* tab,
                                        bool showing = false);
  ~FakeGlicSidePanelCoordinator() override;

  // GlicSidePanelCoordinator:
  void Show(const ShowOptions& options) override;
  void Close(const CloseOptions& options) override;
  bool IsShowing() const override;
  State state() override;
  bool SupportsPeek() const override;
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State)> callback) override;
  void SetContentsView(std::unique_ptr<views::View> contents_view) override;
  int GetPreferredWidth() override;
  bool IsGlicSidePanelActive() override;

  void SetState(State state);

 private:
  State state_;
  base::RepeatingCallbackList<void(State)> state_callbacks_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_FAKE_GLIC_SIDE_PANEL_COORDINATOR_H_
