// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SCRIPT_INJECTION_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SCRIPT_INJECTION_SIGNAL_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "extensions/common/extension_id.h"

namespace safe_browsing {

// A signal that is created when an extension injects a script into a web page.
class ScriptInjectionSignal : public ExtensionSignal {
 public:
  ScriptInjectionSignal(const extensions::ExtensionId& extension_id,
                        std::string api_name,
                        std::string url,
                        std::vector<std::string> args_list,
                        std::string arg_url,
                        base::Time timestamp);
  ~ScriptInjectionSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  // Returns a unique key, which can be used to compare signal arguments and as
  // a key for aggregation in the processor.
  std::string GetAggregationKey() const;

  const std::string& api_name() const { return api_name_; }
  const std::string& url() const { return url_; }
  const std::vector<std::string>& args_list() const { return args_list_; }
  const std::string& arg_url() const { return arg_url_; }
  const base::Time& timestamp() const { return timestamp_; }

 protected:
  std::string api_name_;
  std::string url_;
  std::vector<std::string> args_list_;
  std::string arg_url_;
  base::Time timestamp_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SCRIPT_INJECTION_SIGNAL_H_
