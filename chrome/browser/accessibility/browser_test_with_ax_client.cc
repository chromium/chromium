// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/browser_test_with_ax_client.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/win/windows_handle_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace {

// Converts a BrowserTestWithAxClient::ClientApi to a
// ax_client::Launcher::ClientApi.
ax_client::Launcher::ClientApi ToLauncherClientApi(
    BrowserTestWithAxClient::ClientApi client_api) {
  switch (client_api) {
    case BrowserTestWithAxClient::ClientApi::kUiAutomation:
      return ax_client::Launcher::ClientApi::kUiAutomation;
    case BrowserTestWithAxClient::ClientApi::kIAccessible2:
      return ax_client::Launcher::ClientApi::kIAccessible2;
  }
}

}  // namespace

BrowserTestWithAxClient::BrowserTestWithAxClient() = default;

BrowserTestWithAxClient::~BrowserTestWithAxClient() = default;

void BrowserTestWithAxClient::SetUpOnMainThread() {
  // Enable platform activation since that is what is beign tested here.
  content::BrowserAccessibilityState::GetInstance()
      ->SetActivationFromPlatformEnabled(
          /*enabled=*/true);

  // Launch the AxClient now that IPC support is up and running in the browser.
  auto pending_remote = ax_client_launcher_.Launch(
      ToLauncherClientApi(GetClientApi()),
      base::BindRepeating(&BrowserTestWithAxClient::OnAxClientError,
                          base::Unretained(this)));
  ASSERT_TRUE(pending_remote);
  ax_client_.Bind(std::move(pending_remote));
  ax_client_.set_disconnect_with_reason_handler(base::BindOnce(
      &BrowserTestWithAxClient::OnClientDisconnect, base::Unretained(this)));

  InProcessBrowserTest::SetUpOnMainThread();
}

void BrowserTestWithAxClient::TearDownOnMainThread() {
  // Gracefully shut down the client if the connection is still alive.
  if (ax_client_) {
    base::test::TestFuture<void> client_shutdown;
    ax_client_->Shutdown(client_shutdown.GetCallback());
    ASSERT_TRUE(client_shutdown.Wait());
    ax_client_.reset();
  }

  InProcessBrowserTest::TearDownOnMainThread();
}

HRESULT BrowserTestWithAxClient::InitializeClient(Browser* browser) {
  // Get the HWND of `browser`.
  const HWND browser_hwnd = BrowserView::GetBrowserViewForBrowser(browser)
                                ->GetNativeWindow()
                                ->GetHost()
                                ->GetAcceleratedWidget();

  // Initialize the UI Automation client; giving it this window.
  base::test::TestFuture<uint32_t> future;
  ax_client_->Initialize(base::win::HandleToUint32(browser_hwnd),
                         future.GetCallback());
  return future.Get();
}

HRESULT BrowserTestWithAxClient::FindAll() {
  base::test::TestFuture<uint32_t> future;
  ax_client_->FindAll(future.GetCallback());
  return future.Get();
}

void BrowserTestWithAxClient::TerminateClient() {
  base::test::TestFuture<void> future;
  disconnect_closure_ = future.GetCallback();
  ax_client_->Terminate();
  return future.Get();
}

// static
std::string BrowserTestWithAxClient::NodeCountsToString(
    const ui::AXPlatformNodeWin::Counts& counts) {
  return base::StrCat(
      {"INSTANCE: ", base::NumberToString(counts.base_nodes),
       ", DORMANT: ", base::NumberToString(counts.dormant_nodes),
       ", LIVE: ", base::NumberToString(counts.live_nodes),
       ", GHOST: ", base::NumberToString(counts.ghost_nodes)});
}

// static
void BrowserTestWithAxClient::AssertNoLeakedNodes() {
  auto counts = ui::AXPlatformNodeWin::GetCounts();
  ASSERT_EQ(counts.ghost_nodes, 0ULL) << NodeCountsToString(counts);
}

// static
void BrowserTestWithAxClient::WaitForNodeCounts(
    const ui::AXPlatformNodeWin::Counts& counts) {
  ASSERT_TRUE(base::test::RunUntil([&counts] {
    return ui::AXPlatformNodeWin::GetCounts() == counts;
  })) << "Timeout waiting for expected node counts:"
      << std::endl
      << "  desired: " << NodeCountsToString(counts) << std::endl
      << "   actual: "
      << NodeCountsToString(ui::AXPlatformNodeWin::GetCounts());
}

void BrowserTestWithAxClient::OnAxClientError(const std::string& error) {
  ADD_FAILURE() << error;
}

void BrowserTestWithAxClient::OnClientDisconnect(
    uint32_t custom_reason,
    const std::string& description) {
  ax_client_.reset();

  if (disconnect_closure_) {
    // The disconnect was expected; run the closure.
    std::move(disconnect_closure_).Run();
  } else {
    // The disconnect was not expected; fail the test.
    ADD_FAILURE() << "ax_client disconnected with custom_reason "
                  << custom_reason << " and description \"" << description
                  << "\"";
  }
}
