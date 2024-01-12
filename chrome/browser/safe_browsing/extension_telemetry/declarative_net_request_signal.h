// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_SIGNAL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "extensions/common/api/declarative_net_request.h"

namespace safe_browsing {

// A signal that is created when an extension invokes declarativeNetRequest API.
class DeclarativeNetRequestSignal : public ExtensionSignal {
 public:
  DeclarativeNetRequestSignal(
      const extensions::ExtensionId& extension_id,
      const std::vector<extensions::api::declarative_net_request::Rule>& rules);
  ~DeclarativeNetRequestSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  const std::vector<extensions::api::declarative_net_request::Rule>& rules()
      const {
    return *rules_;
  }

 protected:
  // Rules to be added from the declarativeNetRequest API invocations.
  const raw_ref<
      const std::vector<extensions::api::declarative_net_request::Rule>>
      rules_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_DECLARATIVE_NET_REQUEST_SIGNAL_H_
