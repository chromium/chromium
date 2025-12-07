// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_FORM_FILLING_TOOL_REQUEST_H_

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

class ToolRequestVisitorFunctor;

class AttemptFormFillingToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "AttemptFormFilling";
  // Note: While autofill detects the type of data to be filled into a field
  // (address or credit card), autofill is unable to identify the purpose (e.g.
  // shipping v.s. billing address). Therefore the purpose needs to be provided
  // when showing UI, so that multiple sections of the same type can be
  // disambiguated in the UI.
  //
  // See also RequestedData in actions_data.proto.
  enum class RequestedData {
    // The requested data is not specified.
    kUnknown = 0,

    // An address should be filled. This value can be used as a catch-all when
    // the more specific address options below do not fit.
    kAddress = 1,

    // A shipping address should be filled.
    kShippingAddress = 2,

    // A billing address should be filled.
    kBillingAddress = 3,

    // A home address should be filled.
    kHomeAddress = 4,

    // A work address should be filled.
    kWorkAddress = 5,

    // A credit card should be filled.
    kCreditCard = 6,

    // Contact information should be filled. Contact information includes name,
    // email, phone number, but not postal address information (street, city,
    // etc.)
    kContactInformation = 7,
  };

  struct FormFillingRequest {
    FormFillingRequest();
    ~FormFillingRequest();
    FormFillingRequest(const FormFillingRequest&);
    FormFillingRequest& operator=(const FormFillingRequest&);
    FormFillingRequest(FormFillingRequest&&);
    FormFillingRequest& operator=(FormFillingRequest&&);

    RequestedData requested_data{};
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
