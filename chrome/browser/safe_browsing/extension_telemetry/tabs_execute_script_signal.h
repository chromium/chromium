// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_EXECUTE_SCRIPT_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_EXECUTE_SCRIPT_SIGNAL_H_

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"

namespace safe_browsing {

// A signal that is created when an extension invokes tabs.executeScript API.
class TabsExecuteScriptSignal : public ExtensionSignal {
 public:
  // A SHA-256 Hash is generated from the script code during instantiation.
  TabsExecuteScriptSignal(const extensions::ExtensionId& extension_id,
                          const std::string& script_code);
  ~TabsExecuteScriptSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  // SHA-256 hash of the script code (calculated at object creation time).
  const std::string& script_hash() const { return script_hash_; }

 protected:
  std::string script_hash_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_EXECUTE_SCRIPT_SIGNAL_H_
