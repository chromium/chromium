// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_DICTATION_MENU_OBSERVER_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_DICTATION_MENU_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"

class BrowserWindowInterface;
class RenderViewContextMenuProxy;

namespace dictation {

class DictationKeyedService;

class DictationMenuObserver : public RenderViewContextMenuObserver {
 public:
  explicit DictationMenuObserver(RenderViewContextMenuProxy* proxy,
                                 BrowserWindowInterface* bwi);
  ~DictationMenuObserver() override;

  // RenderViewContextMenuObserver:
  void InitMenu(const content::ContextMenuParams& params) override;
  bool IsCommandIdSupported(int command_id) override;
  bool IsCommandIdEnabled(int command_id) override;
  void ExecuteCommand(int command_id) override;

 private:
  DictationKeyedService* GetDictationService();

  // raw_ptr as the observer cannot outlive the context menu and the context
  // menu cannot outlive the owning window.
  raw_ptr<BrowserWindowInterface> window_;
  raw_ptr<RenderViewContextMenuProxy> proxy_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_DICTATION_MENU_OBSERVER_H_
