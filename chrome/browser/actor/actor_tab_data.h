// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_TAB_DATA_H_
#define CHROME_BROWSER_ACTOR_ACTOR_TAB_DATA_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "ui/gfx/geometry/point.h"

namespace actor {

enum class ApcSource;

namespace ui {
class DomNodeGeometry;
}

// Represents data that the actor service stores for an ActorSurface (tab or
// headless surface). Owned by the surface, so it is kept intact when the
// surface is promoted to a tab or demoted back to headless.
class ActorTabData {
 public:
  ActorTabData();
  ~ActorTabData();

  // Returns the data of the surface identified by `actor_surface_handle`, or
  // nullptr if that surface no longer exists.
  static ActorTabData* From(ActorSurfaceHandle actor_surface_handle);

  // Stores `result`'s page content, along with the internal IDs of the script
  // tools in it (see `AIPageContentResult::script_tool_ids`).
  void DidObserveContent(const optimization_guide::AIPageContentResult& result,
                         ApcSource source);
  // Like the above, for callers that only have the page content proto. No
  // script tool IDs are stored, so actions fall back to looking script tools up
  // by name.
  //
  // DEPRECATED; TODO(http://b/458415347): Convert browser tests to call
  // `DidObserveContent()` with a result struct.
  void DidObserveContent(
      const optimization_guide::proto::AnnotatedPageContent& content,
      ApcSource source);

  // Returns last observed page content, nullptr if no observation has been
  // made.
  const optimization_guide::proto::AnnotatedPageContent*
  GetLastObservedPageContent();

  const ui::DomNodeGeometry* GetLastObservedDomNodeGeometry();

  // Returns the internal ID of the script tool named `tool_name` that the
  // document identified by `document_id` reported in the last observed page
  // content, or nullopt if there is no such tool.
  std::optional<blink::ScriptToolToken> GetLastObservedScriptToolId(
      const base::UnguessableToken& document_id,
      const std::string& tool_name) const;

  // Sets the coordinate that was resolved by the renderer during the tool
  // validation phase.
  void SetLastRendererResolvedTarget(const gfx::Point& point);

  // Retrieves the renderer resolved coordinate.
  std::optional<gfx::Point> GetLastRendererResolvedTarget();

 private:
  // Stores the last observed page content for TOCTOU check.
  std::optional<optimization_guide::proto::AnnotatedPageContent>
      last_observed_page_content_;
  // The internal IDs of the script tools in `last_observed_page_content_`.
  optimization_guide::DocumentIdentifierToScriptTools
      last_observed_script_tool_ids_;
  std::unique_ptr<ui::DomNodeGeometry> last_observed_dom_node_geometry_;
  std::optional<gfx::Point> last_renderer_resolved_target_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_TAB_DATA_H_
