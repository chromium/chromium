// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/type_tool_request.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/actor_features.h"

namespace actor {

using ::tabs::TabHandle;

TypeToolRequest::TypeToolRequest(TabHandle tab_handle,
                                 const PageTarget& target,
                                 std::string_view text,
                                 bool follow_by_enter,
                                 Mode mode)
    : PageToolRequest(tab_handle, target),
      text(text),
      follow_by_enter(follow_by_enter),
      mode(mode) {}

TypeToolRequest::~TypeToolRequest() = default;

void TypeToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view TypeToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr TypeToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto type = mojom::TypeAction::New();

  type->text = text;
  type->follow_by_enter = follow_by_enter;

  switch (mode) {
    case Mode::kReplace:
      type->mode = mojom::TypeAction::Mode::kDeleteExisting;
      break;
    case Mode::kPrepend:
      type->mode = mojom::TypeAction::Mode::kPrepend;
      break;
    case Mode::kAppend:
      type->mode = mojom::TypeAction::Mode::kAppend;
      break;
  }

  return mojom::ToolAction::NewType(std::move(type));
}

std::unique_ptr<PageToolRequest> TypeToolRequest::Clone() const {
  return std::make_unique<TypeToolRequest>(*this);
}

std::string TypeToolRequest::GetTextContentSentToRenderer() const {
  return text;
}

ObservationDelayController::PageStabilityConfig
TypeToolRequest::GetObservationPageStabilityConfig() const {
  ObservationDelayController::PageStabilityConfig config{
      .supports_paint_stability = true,
  };

  // Typing into input fields often causes custom made dropdowns to appear and
  // update content. These are often updated via async tasks that try to detect
  // when a user has finished typing. Delay observation to try to ensure the
  // page stability monitor kicks in only after these tasks have invoked.
  if (base::FeatureList::IsEnabled(kActorTypeToolObservationStartDelay)) {
    config.start_delay = kActorTypeToolObservationStartDelayDuration.Get();
  }
  return config;
}

}  // namespace actor
