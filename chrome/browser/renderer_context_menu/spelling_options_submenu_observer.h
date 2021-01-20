// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLING_OPTIONS_SUBMENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLING_OPTIONS_SUBMENU_OBSERVER_H_

#include <cstddef>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/prefs/pref_member.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "ui/base/models/simple_menu_model.h"

class RenderViewContextMenuProxy;

// A class that implements the 'spell-checker options' submenu. This class
// creates the submenu, adds it to the parent menu, and handles events.
class SpellingOptionsSubMenuObserver : public RenderViewContextMenuObserver {
 public:
  SpellingOptionsSubMenuObserver(RenderViewContextMenuProxy* proxy,
                                 ui::SimpleMenuModel::Delegate* delegate,
                                 int group_id);
  ~SpellingOptionsSubMenuObserver() override;

  // RenderViewContextMenuObserver implementation.
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdChecked(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  // The interface for adding a submenu to the parent.
  RenderViewContextMenuProxy* proxy_;

  // The submenu of the 'spell-checker options'. This class adds items to this
  // submenu and adds it to the parent menu.
  ui::SimpleMenuModel submenu_model_;

  // The ID of radio items representing languages available for spellchecking.
  int language_group_id_;

  // A vector of all dictionaries available for spellchecking.
  std::vector<SpellcheckService::Dictionary> dictionaries_;

  // The number of dictionaries currently selected for spellchecking.
  size_t num_selected_dictionaries_;

  // Flag indicating whether the server-powered spellcheck service is enabled.
  BooleanPrefMember use_spelling_service_;

  DISALLOW_COPY_AND_ASSIGN(SpellingOptionsSubMenuObserver);
};

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_SPELLING_OPTIONS_SUBMENU_OBSERVER_H_
