// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_TEST_WITH_AX_CLIENT_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_TEST_WITH_AX_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "chrome/test/accessibility/ax_client/ax_client.test-mojom.h"
#include "chrome/test/accessibility/ax_client/launcher.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"

// A test fixture that runs an accessibility client (ax_client) in a separate
// process. The fixture asserts that no AXPlatformNode instances are leaked
// during teardown. Tests based on this fixture may interleave operations on
// the browser with those in the client to evaluate interactions that mimic
// real-world scenarios involving assistive tech such as a screen reader.
class BrowserTestWithAxClient : public InProcessBrowserTest {
 public:
  enum class ClientApi {
    // Use the UI Automation client APIs.
    kUiAutomation,

    // Use the IAccessible2 w/ Microsoft Active Accessibility client APIs.
    kIAccessible2,
  };

 protected:
  BrowserTestWithAxClient();
  ~BrowserTestWithAxClient() override;

  // Enables platform-activation of accessibility modes so that native and web
  // accessibility are turned on as a result of the AxClient's interactions with
  // the browser via accessibility calls and launches the ax_client process.
  void SetUpOnMainThread() override;

  // Gracefully shuts down the ax_client process and waits for it to do so.
  void TearDownOnMainThread() override;

  // Gives the top-level HWND of `browser` to ax_client so that it can operate
  // on it using the given client-side API; see
  // `ax_client::mojom::AxClient::Initialize()`.
  HRESULT InitializeClient(Browser* browser);

  // Instructs the client to find all nodes in the window it is watching; see
  // `ax_client::mojom::AxClient::FindAll()`.
  HRESULT FindAll();

  void TerminateClient();

  // Returns the various accessibility node counts for human consumption.
  static std::string NodeCountsToString(
      const ui::AXPlatformNodeWin::Counts& counts);

  // Asserts that there are no leaked AXPlatformNodeWin instances.
  static void AssertNoLeakedNodes();

  // Waits for the node counts to reach `counts`.
  static void WaitForNodeCounts(const ui::AXPlatformNodeWin::Counts& counts);

 private:
  // Returns the type of accessibility API that the test would like the client
  // to use.
  virtual ClientApi GetClientApi() const = 0;

  // Handles a connection error with the client by adding a non-fatal failure
  // to the test run.
  void OnAxClientError(const std::string& error);

  // Handles a disconnect with the remote AxClient instance.
  void OnClientDisconnect(uint32_t custom_reason,
                          const std::string& description);

  // Launches and manages the lifetime of the ax_client process.
  ax_client::Launcher ax_client_launcher_;

  // The connection to ax_client.
  mojo::Remote<ax_client::mojom::AxClient> ax_client_;

  // A callback that will be run when the client disconnects expectedly.
  base::OnceClosure disconnect_closure_;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_BROWSER_TEST_WITH_AX_CLIENT_H_
