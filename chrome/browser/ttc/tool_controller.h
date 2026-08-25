// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_TOOL_CONTROLLER_H_
#define CHROME_BROWSER_TTC_TOOL_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.mojom.h"

class Profile;
class BrowserWindowInterface;

namespace ttc {

class ToolController {
 public:
  explicit ToolController(Profile* profile);
  ~ToolController();

  void OpenUrl(
      BrowserWindowInterface* browser,
      const std::string& url_string,
      bool new_tab,
      ai_overlay_dialog::mojom::AiOverlayTools::OpenUrlCallback callback);

 private:
  void EnsureTaskCreated(actor::ActorKeyedService* actor_service);
  void OnNavigateActionsFinished(
      ai_overlay_dialog::mojom::AiOverlayTools::OpenUrlCallback callback,
      std::vector<actor::ActionResultWithLatencyInfo> results,
      actor::TabObservationStrategy strategy);

  raw_ptr<Profile> profile_;
  actor::TaskId task_id_;
  base::WeakPtrFactory<ToolController> weak_factory_{this};
};

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_TOOL_CONTROLLER_H_
