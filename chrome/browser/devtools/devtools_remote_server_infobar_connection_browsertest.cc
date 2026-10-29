// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/automation_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

class EphemeralDevToolsSocketFactory : public content::DevToolsSocketFactory {
 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    auto socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
    if (socket->ListenWithAddressAndPort("127.0.0.1", 0, 1) != net::OK) {
      return nullptr;
    }
    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }
};

class BrowserTargetDetachedWaiter : public content::DevToolsAgentHostObserver {
 public:
  BrowserTargetDetachedWaiter() {
    content::DevToolsAgentHost::AddObserver(this);
  }
  ~BrowserTargetDetachedWaiter() override {
    content::DevToolsAgentHost::RemoveObserver(this);
  }

  bool Wait() { return detached_.Wait(); }

 private:
  void DevToolsAgentHostDetached(content::DevToolsAgentHost* host) override {
    if (host->GetType() == content::DevToolsAgentHost::kTypeBrowser) {
      detached_.SetValue();
    }
  }

  base::test::TestFuture<void> detached_;
};

class DevToolsRemoteServerInfobarConnectionBrowserTest
    : public InProcessBrowserTest {
 protected:
  DevToolsRemoteServerInfobarConnectionBrowserTest() {
    feature_list_.InitAndDisableFeature(infobars::kCentralizedInfoBarFramework);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // The test page is a WebSocket client on a different loopback port.
    command_line->AppendSwitchASCII(switches::kRemoteAllowOrigins, "*");
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/empty.html")));
    content::DevToolsAgentHost::StartRemoteDebuggingServer(
        std::make_unique<EphemeralDevToolsSocketFactory>(), base::FilePath(),
        base::FilePath(), content::DevToolsAgentHost::kWithApprovalOnly);
    ASSERT_TRUE(base::test::RunUntil([] {
      return !content::DevToolsAgentHost::GetRemoteDebuggingServerAddress()
                  .empty();
    }));
  }

  void TearDownOnMainThread() override {
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  void Connect() {
    base::test::TestFuture<views::Widget*> dialog_shown;
    views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
    observer.set_shown_callback(
        base::BindLambdaForTesting([&](views::Widget* widget) {
          if (widget->widget_delegate()->GetWindowTitle() ==
              l10n_util::GetStringUTF16(
                  IDS_DEV_TOOLS_CONNECTION_DIALOG_TITLE)) {
            dialog_shown.SetValue(widget);
          }
        }));

    ASSERT_TRUE(content::ExecJs(
        contents(),
        content::JsReplace(
            R"(
      window.cdp = new WebSocket($1);
      window.cdpConnected = new Promise((resolve, reject) => {
        cdp.onopen = () => resolve(true);
        cdp.onerror = () => reject(new Error('CDP connection failed'));
      });
      window.cdpCommand = (method, params = {}) => {
        return new Promise((resolve, reject) => {
          cdp.onmessage = event => {
            const response = JSON.parse(event.data);
            if (response.id !== 1) return;
            if (response.error) {
              reject(new Error(JSON.stringify(response.error)));
            } else {
              resolve(response.result);
            }
          };
          cdp.send(JSON.stringify({id: 1, method, params}));
        });
      };
    )",
            "ws://" +
                content::DevToolsAgentHost::GetRemoteDebuggingServerAddress() +
                "/devtools/browser")));

    ASSERT_TRUE(dialog_shown.Wait());
    auto* dialog = dialog_shown.Get()->widget_delegate()->AsDialogDelegate();
    ASSERT_TRUE(dialog);
    dialog->AcceptDialog();
    ASSERT_EQ(true, content::EvalJs(contents(), "cdpConnected"));
  }

  void Disconnect() {
    BrowserTargetDetachedWaiter detached;
    ASSERT_TRUE(content::ExecJs(contents(), "cdp.close()"));
    ASSERT_TRUE(detached.Wait());
    // OnClose() updates the infobar after detaching the browser target.
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DevToolsRemoteServerInfobarConnectionBrowserTest,
                       DisconnectAfterInfoBarCreationFails) {
  auto* manager = infobars::ContentInfoBarManager::FromWebContents(contents());
  auto* automation_infobar = AutomationInfoBarDelegate::Create(manager);
  ASSERT_TRUE(automation_infobar);
  auto automation_infobar_weak = automation_infobar->AsWeakPtr();

  ASSERT_NO_FATAL_FAILURE(Connect());
  // Both delegates use IDS_CONTROLLED_BY_AUTOMATION, so the remote debugging
  // infobar is rejected as a duplicate and its global owner deletes itself.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, manager->infobars().size());
  EXPECT_EQ(infobars::InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE,
            manager->infobars()[0]->delegate()->GetIdentifier());

  ui_test_utils::BrowserDestroyedObserver browser_destroyed;
  ASSERT_EQ(true, content::EvalJs(contents(), R"(
    (async () => {
      const {targetId} = await cdpCommand('Target.createTarget', {
        url: 'about:blank', newWindow: true, background: true
      });
      return (await cdpCommand('Target.closeTarget', {targetId})).success;
    })()
  )"));
  // Target.closeTarget acknowledges the request before the window is destroyed.
  browser_destroyed.Wait();
  ASSERT_NO_FATAL_FAILURE(Disconnect());

  // A new connection must be able to show and close the warning again once
  // the conflicting infobar is gone.
  ASSERT_TRUE(automation_infobar_weak);
  automation_infobar_weak->RemoveSelf();
  ASSERT_NO_FATAL_FAILURE(Connect());
  ASSERT_EQ(1u, manager->infobars().size());
  EXPECT_EQ(
      infobars::InfoBarDelegate::DEV_TOOLS_REMOTE_DEBUGGING_INFOBAR_DELEGATE,
      manager->infobars()[0]->delegate()->GetIdentifier());
  ASSERT_NO_FATAL_FAILURE(Disconnect());
  EXPECT_TRUE(manager->infobars().empty());
}

}  // namespace
