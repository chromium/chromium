// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_ACTION_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_ACTION_SIGNAL_H_

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "url/gurl.h"

namespace safe_browsing {

using DeclarativeNetRequestActionInfo = ExtensionTelemetryReportRequest::
    SignalInfo::DeclarativeNetRequestActionInfo;

// A signal that is created when a web request URL from Chrome matches a
// pattern specified DeclarativeNetRequest rule configured for an extension.
class DeclarativeNetRequestActionSignal : public ExtensionSignal {
 public:
  // Creates a declarativeNetRequest redirect action signal.
  static std::unique_ptr<DeclarativeNetRequestActionSignal>
  CreateDeclarativeNetRequestRedirectActionSignal(
      const extensions::ExtensionId& extension_id,
      const GURL& request_url,
      const GURL& redirect_url);

  ~DeclarativeNetRequestActionSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  // Creates a unique id, which can be used to uniquely identify a
  // declarativeNetRequest action signal.
  std::string GetUniqueActionDetailsId() const;

  const DeclarativeNetRequestActionInfo::ActionDetails& action_details() const {
    return action_details_;
  }

 protected:
  DeclarativeNetRequestActionSignal(
      const extensions::ExtensionId& extension_id,
      const DeclarativeNetRequestActionInfo::ActionDetails& action_details);

  DeclarativeNetRequestActionInfo::ActionDetails action_details_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_ACTION_SIGNAL_H_
