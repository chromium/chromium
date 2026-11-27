// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/attempt_login/attempt_login_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/execution_engine_dispatcher.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/password_manager/attempt_login/attempt_login_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/actor/tools/tool_result.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "content/public/common/content_features.h"

namespace actor {

namespace {

constexpr std::string_view kAttemptLoginToolDescription =
    "Attempts to log in to the website using saved credentials or social login "
    "buttons.";

// Inner-request issued by the dispatcher to create the tool. This derives from
// AttemptLoginToolRequest so that it's treated the same as the composite
// request in places that key off the request's type or name (e.g. UI events,
// journal, metrics, critical action logging).
class AttemptLoginToolRequestInternal : public AttemptLoginToolRequest {
 public:
  AttemptLoginToolRequestInternal(
      ActorSurfaceHandle actor_surface_handle,
      std::optional<PageTarget> password_button,
      std::optional<PageTarget> sign_in_with_google_button)
      : AttemptLoginToolRequest(actor_surface_handle,
                                std::move(password_button),
                                std::move(sign_in_with_google_button)) {}
  ~AttemptLoginToolRequestInternal() override = default;

  // ToolRequest:
  std::unique_ptr<ExecutionEngineDispatcher> CreateDispatcher() const override {
    return nullptr;
  }

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override {
    ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
    if (!actor_surface) {
      return {/*tool=*/nullptr,
              MakeResult(mojom::ActionResultCode::kTabWentAway,
                         /*requires_page_stabilization=*/false,
                         "The target surface is no longer present.")};
    }

    return {std::make_unique<AttemptLoginTool>(
                task_id, tool_delegate, *actor_surface, password_button(),
                sign_in_with_google_button()),
            MakeOkResult()};
  }
};

// Sequences the AttemptLoginTool with a follow up ClickTool to complete the
// login, if needed. The results of both are merged and reported as a single
// action.
class AttemptLoginDispatcher : public ExecutionEngineDispatcher {
 public:
  AttemptLoginDispatcher(ActorSurfaceHandle actor_surface_handle,
                         std::optional<PageTarget> password_button,
                         std::optional<PageTarget> sign_in_with_google_button,
                         bool requires_opening_web_contents,
                         bool skip_observation_delay)
      : actor_surface_handle_(actor_surface_handle),
        password_button_(std::move(password_button)),
        sign_in_with_google_button_(std::move(sign_in_with_google_button)),
        requires_opening_web_contents_(requires_opening_web_contents),
        skip_observation_delay_(skip_observation_delay) {}
  ~AttemptLoginDispatcher() override = default;

  // ExecutionEngineDispatcher:
  NextAction GetNext(ToolDelegate& tool_delegate) override {
    switch (state_) {
      case State::kInitial: {
        state_ = State::kAttemptingLogin;
        auto request = std::make_unique<AttemptLoginToolRequestInternal>(
            actor_surface_handle_, std::move(password_button_),
            std::move(sign_in_with_google_button_));
        request->SetSkipObservationDelay(skip_observation_delay_);
        return request;
      }
      case State::kAttemptingLogin: {
        CHECK(result_);
        const ToolResult& login_result = result_->tool_result;
        if (!login_result.Ok() || !login_result.data()) {
          state_ = State::kDone;
          return TakeResult();
        }
        state_ = State::kClicking;
        auto request = std::make_unique<ClickToolRequest>(
            actor_surface_handle_,
            login_result.data()->As<AttemptLoginToolResult>()->button_to_click,
            mojom::ClickType::kLeft, mojom::ClickCount::kSingle,
            requires_opening_web_contents_,
            AttemptLoginToolRequest::GetLoginObservationPageStabilityConfig());
        request->SetSkipObservationDelay(skip_observation_delay_);
        return request;
      }
      case State::kClicking:
        state_ = State::kDone;
        return TakeResult();
      case State::kDone:
        NOTREACHED();
    }
  }

  void OnToolCompleted(ToolResultWithLatencyInfo result) override {
    switch (state_) {
      case State::kAttemptingLogin:
        CHECK(!result_);
        result_ = std::move(result);
        break;
      case State::kClicking:
        CHECK(result_);
        MergeFollowupResult(*result_, std::move(result));
        break;
      case State::kInitial:
      case State::kDone:
        NOTREACHED();
    }
  }

 private:
  enum class State {
    kInitial,
    kAttemptingLogin,
    kClicking,
    kDone,
  };

  ToolResultWithLatencyInfo TakeResult() {
    CHECK(result_);
    ToolResultWithLatencyInfo result = std::move(*result_);
    result_.reset();
    return result;
  }

  State state_ = State::kInitial;
  ActorSurfaceHandle actor_surface_handle_;
  std::optional<PageTarget> password_button_;
  std::optional<PageTarget> sign_in_with_google_button_;
  const bool requires_opening_web_contents_;
  const bool skip_observation_delay_;
  std::optional<ToolResultWithLatencyInfo> result_;
};

}  // namespace

AttemptLoginToolRequest::AttemptLoginToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    std::optional<PageTarget> password_button,
    std::optional<PageTarget> sign_in_with_google_button)
    : TabToolRequest(actor_surface_handle),
      password_button_(password_button),
      sign_in_with_google_button_(sign_in_with_google_button) {}

AttemptLoginToolRequest::~AttemptLoginToolRequest() = default;

// static
std::optional<ToolDefinition> AttemptLoginToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kAttemptLogin, kModelFacingName,
                               kAttemptLoginToolDescription)
      .Build();
}

AttemptLoginToolRequest::AttemptLoginToolRequest(
    const AttemptLoginToolRequest&) = default;
AttemptLoginToolRequest& AttemptLoginToolRequest::operator=(
    const AttemptLoginToolRequest&) = default;

std::unique_ptr<ExecutionEngineDispatcher>
AttemptLoginToolRequest::CreateDispatcher() const {
  return std::make_unique<AttemptLoginDispatcher>(
      GetActorSurfaceHandle(), password_button_, sign_in_with_google_button_,
      RequiresOpeningWebContents(), ShouldSkipObservationDelay());
}

ToolRequest::CreateToolResult AttemptLoginToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  // The tool is created by AttemptLoginToolRequestInternal from the
  // dispatcher.
  NOTREACHED();
}

void AttemptLoginToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view AttemptLoginToolRequest::Name() const {
  return kName;
}

bool AttemptLoginToolRequest::RequiresOpeningWebContents() const {
  return base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin);
}

ObservationDelayController::PageStabilityConfig
AttemptLoginToolRequest::GetLoginObservationPageStabilityConfig() {
  ObservationDelayController::PageStabilityConfig config{
      .supports_paint_stability = true,
  };
  if (base::FeatureList::IsEnabled(kActorLoginObservationStartDelay)) {
    config.start_delay = kActorLoginObservationStartDelayDuration.Get();
  }
  return config;
}

ObservationDelayController::PageStabilityConfig
AttemptLoginToolRequest::GetObservationPageStabilityConfig() const {
  return GetLoginObservationPageStabilityConfig();
}

}  // namespace actor
