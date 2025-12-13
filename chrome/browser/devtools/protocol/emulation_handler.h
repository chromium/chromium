// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/devtools/protocol/emulation.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/devtools_agent_host.h"

class EmulationHandler : public protocol::Emulation::Backend,
                         public infobars::InfoBarManager::Observer {
 public:
  EmulationHandler(content::DevToolsAgentHost* agent_host,
                   protocol::UberDispatcher* dispatcher);
  ~EmulationHandler() override;

  EmulationHandler(const EmulationHandler&) = delete;
  EmulationHandler& operator=(const EmulationHandler&) = delete;

  // Emulation::Backend:
  protocol::Response Disable() override;
  protocol::Response SetAutomationOverride(bool enabled) override;
  protocol::Response GetScreenInfos(
      std::unique_ptr<protocol::Array<protocol::Emulation::ScreenInfo>>*
          out_screen_infos) override;
  protocol::Response AddScreen(
      int left,
      int top,
      int width,
      int height,
      std::unique_ptr<protocol::Emulation::WorkAreaInsets> work_area_insets,
      std::optional<double> device_pixel_ratio,
      std::optional<int> rotation,
      std::optional<int> color_depth,
      std::optional<protocol::String> label,
      std::optional<bool> is_internal,
      std::unique_ptr<protocol::Emulation::ScreenInfo>* out_screen_info)
      override;
  protocol::Response RemoveScreen(const protocol::String& screen_id) override;

  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

 private:
  infobars::ContentInfoBarManager* GetContentInfoBarManager();

  raw_ptr<content::DevToolsAgentHost> agent_host_;
  raw_ptr<infobars::InfoBar> automation_info_bar_ = nullptr;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
