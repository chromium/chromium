// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_AUTOFILL_ATTEMPT_FORM_FILLING_ATTEMPT_FORM_FILLING_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_AUTOFILL_ATTEMPT_FORM_FILLING_ATTEMPT_FORM_FILLING_TOOL_REQUEST_H_

#include <array>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "components/actor/core/shared_types.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"

namespace actor {

class ToolRequestVisitorFunctor;

class AttemptFormFillingToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "AttemptFormFilling";
  static constexpr std::string_view kModelFacingName = "attempt_form_filling";
  static constexpr std::string_view kDomNodeIdParam = "dom_node_id";
  static constexpr std::string_view kRequestedDataParam = "requested_data";
  static constexpr std::array<std::string_view, 3> kRequestedDataValues = {
      "address", "credit_card", "login"};
  using RequestedData = autofill::ActorFormFillingRequestedData;

  struct FormFillingRequest {
    FormFillingRequest();
    ~FormFillingRequest();
    FormFillingRequest(const FormFillingRequest&);
    FormFillingRequest& operator=(const FormFillingRequest&);
    FormFillingRequest(FormFillingRequest&&);
    FormFillingRequest& operator=(FormFillingRequest&&);

    RequestedData requested_data{};
    std::string section_label;
    std::vector<PageTarget> trigger_fields;
  };

  // `credit_card_opaque_token` is an empty string unless a token for a credit
  // card has been generated ahead of time and the user does not get to choose
  // what data to fill.
  AttemptFormFillingToolRequest(ActorSurfaceHandle actor_surface_handle,
                                std::vector<FormFillingRequest> requests,
                                std::string credit_card_opaque_token,
                                bool enqueued_click = false);
  AttemptFormFillingToolRequest(const AttemptFormFillingToolRequest&);
  AttemptFormFillingToolRequest& operator=(
      const AttemptFormFillingToolRequest&);
  ~AttemptFormFillingToolRequest() override;

  // Returns the `ToolId::kAttemptFormFilling` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;

  const std::vector<FormFillingRequest>& requests() const { return requests_; }

  const std::vector<FormFillingRequest>& GetRequestsForTesting() const {
    return requests_;
  }

  const std::string& credit_card_opaque_token() const {
    return credit_card_opaque_token_;
  }

  bool enqueued_click() const { return enqueued_click_; }

 private:
  std::vector<FormFillingRequest> requests_;
  std::string credit_card_opaque_token_;
  // Set to true if a click has already been enqueued for the target field of
  // this request. This prevents infinite click-delegation loops.
  bool enqueued_click_ = false;
};

// To support JournalDetailsBuilder which calls base::ToString(), implement the
// ostream operator<<.
std::ostream& operator<<(
    std::ostream& out,
    const AttemptFormFillingToolRequest::FormFillingRequest& request);

std::ostream& operator<<(
    std::ostream& out,
    const std::vector<AttemptFormFillingToolRequest::FormFillingRequest>&
        requests);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_AUTOFILL_ATTEMPT_FORM_FILLING_ATTEMPT_FORM_FILLING_TOOL_REQUEST_H_
