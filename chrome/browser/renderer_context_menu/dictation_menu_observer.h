// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_DICTATION_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_DICTATION_MENU_OBSERVER_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

class RenderViewContextMenuProxy;

namespace dictation {

class DictationKeyedService;

class DictationMenuObserver : public RenderViewContextMenuObserver {
 public:
  explicit DictationMenuObserver(RenderViewContextMenuProxy* proxy);
  ~DictationMenuObserver() override;

  // RenderViewContextMenuObserver:
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  DictationKeyedService* GetDictationService();

  // raw_ref as the observer cannot outlive the context menu.
  base::raw_ref<RenderViewContextMenuProxy> proxy_;
  std::u16string selection_text_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_DICTATION_MENU_OBSERVER_H_
