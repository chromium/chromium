// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_BASE_H_

#include <optional>

#include "base/feature_list.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace enterprise_connectors {

class ContentAnalysisDelegateBase {
 public:
  virtual ~ContentAnalysisDelegateBase() = default;

  // Called when the user decides to bypass the verdict they obtained from DLP.
  virtual void BypassWarnings(
      std::optional<std::u16string> user_justification) = 0;

  // Called when the user hits "cancel" on the dialog, typically cancelling a
  // pending file transfer.
  virtual void Cancel(bool warning) = 0;

  // Returns the custom message specified by the admin to display in the dialog,
  // or std::nullopt if there isn't any.
  virtual std::optional<std::u16string> GetCustomMessage() const = 0;

  // Returns the custom "learn more" URL specified by the admin to display in
  // the dialog, or std::nullopt if there isn't any.
  virtual std::optional<GURL> GetCustomLearnMoreUrl() const = 0;

  // Returns ranges and associated url link specified for the custom rule
  // message specified by the admin to display in the dialog, or std::nullopt if
  // there isn't any.
  virtual std::optional<std::vector<std::pair<gfx::Range, GURL>>>
  GetCustomRuleMessageRanges() const = 0;

  // Returns true if the final verdict is from a type of analysis that requires
  // user justification to bypass, as per the connector policy.
  virtual bool BypassRequiresJustification() const = 0;

  virtual std::u16string GetBypassJustificationLabel() const = 0;

  // Returns the text to display on the "cancel" button in the dialog, or
  // std::nullopt if no text is specified by this delegate. Takes precedence
  // over any other text that would be chosen by the dialog.
  virtual std::optional<std::u16string> OverrideCancelButtonText() const = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_DELEGATE_BASE_H_
