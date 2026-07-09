// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_REQUEST_H_

#include <iosfwd>
#include <string_view>
#include <vector>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/actor/core/shared_types.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Tool request for attempting one-time password (OTP) filling on a tab.
// The Actor framework uses this to create an AttemptOtpFillingTool.
class AttemptOtpFillingToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "AttemptOtpFilling";

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(AttemptOtpFillingPredictedOtpType)
  enum class OtpType {
    kUnknown = 0,
    kSms = 1,
    kEmail = 2,
    kAuthenticatorApp = 3,
    kMaxValue = kAuthenticatorApp,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/one_time_tokens/enums.xml:AttemptOtpFillingPredictedOtpType)

  AttemptOtpFillingToolRequest(
      tabs::TabHandle tab_handle,
      std::vector<PageTarget> trigger_fields,
      bool for_signin,
      OtpType predicted_otp_type = OtpType::kUnknown);
  AttemptOtpFillingToolRequest(const AttemptOtpFillingToolRequest&);
  AttemptOtpFillingToolRequest& operator=(const AttemptOtpFillingToolRequest&);

  ~AttemptOtpFillingToolRequest() override;

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;

  std::string_view Name() const override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  bool GetForSigninForTesting() const { return for_signin_; }

  const std::vector<PageTarget>& GetTriggerFieldsForTesting() const {
    return trigger_fields_;
  }

  OtpType GetPredictedOtpTypeForTesting() const { return predicted_otp_type_; }

 private:
  std::vector<PageTarget> trigger_fields_;
  bool for_signin_;
  OtpType predicted_otp_type_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ATTEMPT_OTP_FILLING_TOOL_REQUEST_H_
