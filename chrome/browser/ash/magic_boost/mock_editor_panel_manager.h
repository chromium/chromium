// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_EDITOR_PANEL_MANAGER_H_
#define CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_EDITOR_PANEL_MANAGER_H_

#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockEditorPanelManager : public input_method::EditorPanelManager {
 public:
  MockEditorPanelManager();
  MockEditorPanelManager(const MockEditorPanelManager&) = delete;
  MockEditorPanelManager& operator=(const MockEditorPanelManager&) = delete;
  ~MockEditorPanelManager() override;

  // input_method::EditorPanelManager:
  MOCK_METHOD(void,
              GetEditorPanelContext,
              (base::OnceCallback<void(crosapi::mojom::EditorPanelContextPtr)>),
              (override));
  MOCK_METHOD(void, OnPromoCardDeclined, (), (override));
  MOCK_METHOD(void, OnConsentApproved, (), (override));
  MOCK_METHOD(void, OnConsentRejected, (), (override));
  MOCK_METHOD(void, StartEditingFlow, (), (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_EDITOR_PANEL_MANAGER_H_
