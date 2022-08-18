// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_H_

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "url/gurl.h"

namespace safe_browsing {

// A signal that is created when an extension initiates a web request.
class RemoteHostContactedSignal : public ExtensionSignal {
 public:
  RemoteHostContactedSignal(const extensions::ExtensionId& extension_id,
                            const GURL& host_url);
  ~RemoteHostContactedSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  const GURL& contacted_host_url() const { return contacted_host_url_; }

 protected:
  // Url of the remote contacted host.
  GURL contacted_host_url_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_REMOTE_HOST_CONTACTED_SIGNAL_H_
