// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_RELAUNCHER_H_
#define CHROME_BROWSER_MAC_RELAUNCHER_H_

// mac_relauncher implements main browser application relaunches on the Mac.
// When a browser wants to relaunch itself, it can't simply fork off a new
// process and exec a new browser from within. That leaves open a window
// during which two browser applications might be running concurrently. If
// that happens, each will wind up with a distinct Dock icon, which is
// especially bad if the user expected the Dock icon to be persistent by
// choosing Keep in Dock from the icon's contextual menu.
//
// mac_relauncher approaches this problem by introducing an intermediate
// process (the "relauncher") in between the original browser ("parent") and
// replacement browser ("relaunched"). The helper executable is used for the
// relauncher process; because it's an LSUIElement, it doesn't get a Dock
// icon and isn't visible as a running application at all. The parent will
// start a relauncher process, giving it the "writer" side of a pipe that it
// retains the "reader" end of. When the relauncher starts up, it will
// establish a kqueue to wait for the parent to exit, and will then write to
// the pipe. The parent, upon reading from the pipe, is free to exit. When the
// relauncher is notified via its kqueue that the parent has exited, it
// proceeds, launching the relaunched process. The handshake to synchronize
// the parent with the relauncher is necessary to avoid races: the relauncher
// needs to be sure that it's monitoring the parent and not some other process
// in light of PID reuse, so the parent must remain alive long enough for the
// relauncher to set up its kqueue.

#include <string>
#include <vector>

namespace base {
class FilePath;
}

namespace content {
struct MainFunctionParams;
}

namespace mac_relauncher {

// Relaunches the application using the helper application associated with the
// currently running instance of Chrome in the parent browser process as the
// executable for the relauncher process. `args` is an argv-style vector of
// command line arguments of the form normally passed to execv, to be passed to
// the new instance of Chrome as it is launched. `args[0]` is ignored, as this
// always relaunches the installation of Chrome used by the currently running
// instance. If a different installation of Chrome needs to be launched, use
// RelaunchAppAtPathWithHelper below.
//
// The caller should exit soon after RelaunchApp returns successfully. Returns
// true on success, although some failures can occur after this function returns
// true if, for example, they occur within the relauncher process. Returns false
// when the relaunch definitely failed.
bool RelaunchApp(const std::vector<std::string>& args);

// Identical to RelaunchApp, but uses `helper` as the path to use for the
// relauncher process, launches the app at `app_bundle`, and allows additional
// arguments to be supplied to the relauncher process in `relauncher_args`.
// `args[0]` is ignored in favor of `app_bundle`.
//
// The specified helper must be from the same version of Chrome as the running
// parent browser process, as there are no guarantees that the parent and
// relauncher processes from different versions will be able to communicate with
// one another. This variant can be useful to relaunch the same version of
// Chrome from another location, using that location's helper.
bool RelaunchAppAtPathWithHelper(
    const base::FilePath& helper,
    const base::FilePath& app_bundle,
    const std::vector<std::string>& relauncher_args,
    const std::vector<std::string>& args);

namespace internal {

// The entry point from ChromeMain into the relauncher process. This is not a
// user API. Don't call it if your name isn't ChromeMain.
int RelauncherMain(content::MainFunctionParams main_parameters);

}  // namespace internal

}  // namespace mac_relauncher

#endif  // CHROME_BROWSER_MAC_RELAUNCHER_H_
