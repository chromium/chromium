// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/devtools/protocol/emulation.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/devtools_agent_host.h"

class EmulationHandler : public protocol::Emulation::Backend {
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
  protocol::Response UpdateScreen(
      const protocol::String& screen_id,
      std::optional<int> left,
      std::optional<int> top,
      std::optional<int> width,
      std::optional<int> height,
      std::unique_ptr<protocol::Emulation::WorkAreaInsets> work_area_insets,
      std::optional<double> device_pixel_ratio,
      std::optional<int> rotation,
      std::optional<int> color_depth,
      std::optional<protocol::String> label,
      std::optional<bool> is_internal,
      std::unique_ptr<protocol::Emulation::ScreenInfo>* out_screen_info)
      override;
  protocol::Response RemoveScreen(const protocol::String& screen_id) override;
  protocol::Response SetPrimaryScreen(
      const protocol::String& screen_id) override;

 private:
  infobars::ContentInfoBarManager* GetContentInfoBarManager();

  raw_ptr<content::DevToolsAgentHost> agent_host_;
  base::WeakPtr<infobars::InfoBar> automation_info_bar_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_EMULATION_HANDLER_H_
