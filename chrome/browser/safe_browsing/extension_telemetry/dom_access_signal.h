// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DOM_ACCESS_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DOM_ACCESS_SIGNAL_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "extensions/common/extension_id.h"

namespace safe_browsing {

// A signal that is created when an extension accesses the DOM of a web page.
class DOMAccessSignal : public ExtensionSignal {
 public:
  using DOMAccess =
      ExtensionTelemetryReportRequest::SignalInfo::DOMAccessInfo::DOMAccess;
  using AccessType = DOMAccess::AccessType;

  DOMAccessSignal(const extensions::ExtensionId& extension_id,
                  std::string api_name,
                  std::string url,
                  AccessType access_type,
                  base::Time timestamp);
  ~DOMAccessSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  // Returns a unique key, which can be used to compare signal arguments and as
  // a key for aggregation in the processor.
  std::string GetAggregationKey() const;

  const std::string& api_name() const { return api_name_; }
  const std::string& url() const { return url_; }
  AccessType access_type() const { return access_type_; }
  const base::Time& timestamp() const { return timestamp_; }

 protected:
  std::string api_name_;
  std::string url_;
  AccessType access_type_;
  base::Time timestamp_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DOM_ACCESS_SIGNAL_H_
