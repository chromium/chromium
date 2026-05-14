// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_REQUEST_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/actor/core/shared_types.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"

namespace actor {

class ToolRequestVisitorFunctor;

class AttemptFormFillingToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "AttemptFormFilling";
  using RequestedData = autofill::ActorFormFillingRequestedData;

  struct FormFillingRequest {
    FormFillingRequest();
    ~FormFillingRequest();
    FormFillingRequest(const FormFillingRequest&);
    FormFillingRequest& operator=(const FormFillingRequest&);
    FormFillingRequest(FormFillingRequest&&);
    FormFillingRequest& operator=(FormFillingRequest&&);

    RequestedData requested_data{};
    // TODO(crbug.com/502157873): Pass section_label to the tool logic.
    std::string section_label;
    std::vector<PageTarget> trigger_fields;
  };

  AttemptFormFillingToolRequest(tabs::TabHandle tab_handle,
                                std::vector<FormFillingRequest> requests);
  AttemptFormFillingToolRequest(const AttemptFormFillingToolRequest&);
  AttemptFormFillingToolRequest& operator=(
      const AttemptFormFillingToolRequest&);
  ~AttemptFormFillingToolRequest() override;

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;

  const std::vector<FormFillingRequest>& GetRequestsForTesting() const {
    return requests_;
  }

 private:
  std::vector<FormFillingRequest> requests_;
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

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_REQUEST_H_
