// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"

class GURL;

namespace content {
class Page;
}  // namespace content

namespace page_content_annotations {
class PageContextFetcher;
struct FetchPageContextErrorDetails;
struct FetchPageContextResult;
}  // namespace page_content_annotations

namespace actor {

class ActorSurface;

// Captures a screenshot of the active viewport of a tab.
class GetScreenshotTool : public Tool {
 public:
  GetScreenshotTool(TaskId task_id,
                    ToolDelegate& tool_delegate,
                    ActorSurface& actor_surface,
                    base::WeakPtr<content::Page> target_page);
  ~GetScreenshotTool() override;

  // Tool:
  void Validate(DeprecatedToolCallback callback) override;
  mojom::ActionResultPtr TimeOfUseValidation(
      const optimization_guide::proto::AnnotatedPageContent* last_observation)
      override;
  void Invoke(ToolResultCallback callback) override;
  void Cancel() override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  GURL JournalURL() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  std::optional<mojom::ActionResultCode> CheckTargetPage() const;

  void OnScreenshotFetched(
      ToolResultCallback callback,
      base::expected<
          std::unique_ptr<page_content_annotations::FetchPageContextResult>,
          page_content_annotations::FetchPageContextErrorDetails> result);

  ActorSurfaceHandle actor_surface_handle_;
  base::WeakPtr<content::Page> target_page_;
  std::unique_ptr<page_content_annotations::PageContextFetcher>
      page_context_fetcher_;
  base::WeakPtrFactory<GetScreenshotTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_H_
