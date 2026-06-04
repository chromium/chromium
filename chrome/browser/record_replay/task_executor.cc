// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/task_executor.h"

#include <algorithm>
#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/tabs/public/tab_interface.h"

namespace record_replay {

void TaskExecutor::ExecuteTask(
    Profile* profile,
    BrowserWindowInterface* browser_window,
    const TaskDefinition& definition,
    const std::vector<TaskParameter>& parameter_values) {
  if (!profile || !browser_window) {
    return;
  }

  glic::GlicKeyedService* glic_service = glic::GlicKeyedService::Get(profile);
  if (!glic_service) {
    return;
  }

  // 1. Create the prompt string from TaskDefinition and TaskParameterValues.
  std::string prompt = base::StringPrintf(
      "Perform this task for me: \"%s\".\n"
      "Instructions: %s\n\n",
      definition.title().c_str(), definition.description().c_str());

  if (definition.task_steps_size() > 0) {
    prompt += "Steps to follow:\n";
    for (const auto& step : definition.task_steps()) {
      prompt +=
          base::StringPrintf("- Step %d: %s (URL: %s)\n", step.step_index() + 1,
                             step.description().c_str(), step.url().c_str());

      // Get parameter values for this step.
      for (const auto& param : step.parameters()) {
        auto pred_it = std::find_if(
            parameter_values.begin(), parameter_values.end(),
            [&param](const TaskParameter& p) {
              return p.has_id() && param.has_id() && p.id() == param.id();
            });
        if (pred_it != parameter_values.end() && pred_it->has_value()) {
          prompt += base::StringPrintf("  * %s: %s\n", param.name().c_str(),
                                       pred_it->value().c_str());
        }
      }
    }
  }

  // 2. Invoke Glic targeting the active tab's side panel.
  glic::GlicInvokeOptions options(glic::mojom::InvocationSource::kNudge);
  options.prompts = {std::move(prompt)};
  if (tabs::TabInterface* active_tab =
          browser_window->GetActiveTabInterface()) {
    options.target = glic::Target(*active_tab);
  } else {
    options.target = glic::Target(browser_window);
  }

  glic_service->Invoke(std::move(options));
}

}  // namespace record_replay
