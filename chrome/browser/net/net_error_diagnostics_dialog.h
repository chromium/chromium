// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_H_
#define CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_H_

#include <string>

namespace content {
class WebContents;
}

// Returns true if a tool for diagnosing network errors encountered when
// requesting URLs can be shown for the provided WebContents. The ability to
// show the diagnostic tool depends on the host platform, and whether the
// WebContents is incognito.
bool CanShowNetworkDiagnosticsDialog(content::WebContents* web_contents);

// Shows a dialog for investigating an error received when requesting
// |failed_url|.  May only be called when CanShowNetworkDiagnosticsDialog()
// returns true.  The caller is responsible for sanitizing the url.
void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const std::string& failed_url);

#endif  // CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_H_

