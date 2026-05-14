// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_BOCA_UI_PROVIDER_TAB_INFO_COLLECTOR_H_
#define ASH_WEBUI_BOCA_UI_PROVIDER_TAB_INFO_COLLECTOR_H_

#include <memory>

#include "ash/public/cpp/tab_strip_delegate.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "content/public/browser/web_ui.h"

namespace mojom = ash::boca::mojom;

namespace ash::boca {
class TabInfoCollector {
 public:
  static std::unique_ptr<TabInfoCollector> Create(content::WebUI* web_ui,
                                                  bool is_producer);
  static std::unique_ptr<TabInfoCollector> Create(bool is_producer);

  virtual ~TabInfoCollector() = default;

  // Fetches window tab info based on current boca role synchronously.
  virtual std::vector<mojom::WindowPtr> GetWindowTabInfo() = 0;

  // Fetches window tab info for provided `target_window` synchronously.
  virtual std::vector<mojom::WindowPtr> GetWindowTabInfoForTarget(
      aura::Window* target_window) = 0;

  // Fetches window tab info for all browser windows synchronously.
  virtual std::vector<mojom::WindowPtr>
  GetWindowTabInfoForAllBrowserWindows() = 0;
};

}  // namespace ash::boca
#endif  // ASH_WEBUI_BOCA_UI_PROVIDER_TAB_INFO_COLLECTOR_H_
