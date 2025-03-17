// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_EDITOR_PANEL_MANAGER_H_
#define CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_EDITOR_PANEL_MANAGER_H_

#include "chrome/browser/ash/input_method/editor_panel_manager.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockEditorPanelManager : public input_method::EditorPanelManager {
 public:
  MockEditorPanelManager();
  MockEditorPanelManager(const MockEditorPanelManager&) = delete;
  MockEditorPanelManager& operator=(const MockEditorPanelManager&) = delete;
  ~MockEditorPanelManager() override;

  // input_method::EditorPanelManagerImpl:
  MOCK_METHOD(
      void,
      GetEditorPanelContext,
      (base::OnceCallback<void(const chromeos::editor_menu::EditorContext&)>),
      (override));
  MOCK_METHOD(void, OnPromoCardDismissed, (), (override));
  MOCK_METHOD(void, OnPromoCardDeclined, (), (override));
  MOCK_METHOD(void, OnConsentRejected, (), (override));
  MOCK_METHOD(void, StartEditingFlow, (), (override));
  MOCK_METHOD(void,
              StartEditingFlowWithPreset,
              (const std::string& text_query_id),
              (override));
  MOCK_METHOD(void,
              StartEditingFlowWithFreeform,
              (const std::string& text),
              (override));
  MOCK_METHOD(void, OnEditorMenuVisibilityChanged, (bool visible), (override));
  MOCK_METHOD(void,
              LogEditorMode,
              (chromeos::editor_menu::EditorMode mode),
              (override));
  MOCK_METHOD(void, OnConsentApproved, (), (override));
  MOCK_METHOD(void, OnMagicBoostPromoCardDeclined, (), (override));
  MOCK_METHOD(bool, ShouldOptInEditor, (), (override));
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAGIC_BOOST_MOCK_EDITOR_PANEL_MANAGER_H_
