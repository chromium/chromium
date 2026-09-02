// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

// A helper WebUIMessageHandler that quits a RunLoop when a "quit" message
// is received. Used to synchronize WebUI messages in tests.
class WebUIQuitHandler : public content::WebUIMessageHandler {
 public:
  explicit WebUIQuitHandler(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  WebUIQuitHandler(const WebUIQuitHandler&) = delete;
  WebUIQuitHandler& operator=(const WebUIQuitHandler&) = delete;

  ~WebUIQuitHandler() override = default;

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "quit", base::BindRepeating(&WebUIQuitHandler::HandleQuit,
                                    base::Unretained(this)));
  }

 private:
  void HandleQuit(const base::ListValue& args) {
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  base::OnceClosure quit_closure_;
};

class BrowserContextShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static BrowserContextShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<BrowserContextShutdownNotifierFactory> s_factory;
    return s_factory.get();
  }

  BrowserContextShutdownNotifierFactory(
      const BrowserContextShutdownNotifierFactory&) = delete;
  BrowserContextShutdownNotifierFactory& operator=(
      const BrowserContextShutdownNotifierFactory&) = delete;

 private:
  friend class base::NoDestructor<BrowserContextShutdownNotifierFactory>;
  BrowserContextShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "ProfileShutdownWaiter") {}

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextOwnInstance(context);
  }
};

class ProfileShutdownWaiter {
 public:
  ProfileShutdownWaiter(Profile* profile, base::OnceClosure shutdown_callback)
      : profile_(profile), shutdown_callback_(std::move(shutdown_callback)) {
    SubscribeToProfileShutdownNotifier();
    extension_service_ = ExtensionSystem::Get(profile)->extension_service();
  }

  ~ProfileShutdownWaiter() = default;

  void SubscribeToProfileShutdownNotifier() {
    shutdown_subscription_ =
        BrowserContextShutdownNotifierFactory::GetInstance()
            ->Get(profile_)
            ->Subscribe(base::BindRepeating(&ProfileShutdownWaiter::Shutdown,
                                            base::Unretained(this)));
  }

  void Wait() { run_loop_.Run(); }

  void Shutdown() {
    ASSERT_TRUE(extension_service_);
    ASSERT_TRUE(extension_service_->HasShutDownExecutedForTest());
    run_loop_.Quit();
    profile_ = nullptr;
    extension_service_ = nullptr;
    if (shutdown_callback_) {
      std::move(shutdown_callback_).Run();
    }
  }

  static void EnsureShutdownNotifierFactoryBuilt() {
    BrowserContextShutdownNotifierFactory::GetInstance();
  }

 private:
  base::CallbackListSubscription shutdown_subscription_;
  base::RunLoop run_loop_;
  raw_ptr<Profile> profile_;
  base::OnceClosure shutdown_callback_;
  raw_ptr<ExtensionService> extension_service_;
};

class ExtensionServiceBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionServiceBrowserTest() {
    ProfileShutdownWaiter::EnsureShutdownNotifierFactoryBuilt();
  }
  ~ExtensionServiceBrowserTest() override = default;
};

// ChromeOS does not support multiple profiles. Hence excluding this test from
// ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
// Tests that ExtensionService does not observe host events after
// ExtensionService::Shutdown() has been executed.
IN_PROC_BROWSER_TEST_F(ExtensionServiceBrowserTest,
                       NoHostObservationAfterShutDown) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // Create a new profile
  base::FilePath profile_path =
      profile_manager->user_data_dir().AppendASCII("TestProfile");
  auto* profile =
      &profiles::testing::CreateProfileSync(profile_manager, profile_path);
  ASSERT_TRUE(profile);

  std::unique_ptr<content::WebContents> web_contents;

  base::OnceClosure reset_web_contents =
      base::BindOnce(&std::unique_ptr<content::WebContents>::reset,
                     base::Unretained(&web_contents), nullptr);

  ProfileShutdownWaiter profile_shutdown_waiter(profile,
                                                std::move(reset_web_contents));

  // Create a standalone WebContents for the profile
  web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  // Navigate to a URL to ensure render process is created
  ASSERT_TRUE(content::NavigateToURL(web_contents.get(), GURL("about:blank")));
  content::WaitForLoadStop(web_contents.get());

  // Create a browser for the profile
  BrowserWindowInterface* new_browser = CreateBrowser(profile);
  ASSERT_TRUE(new_browser);

  // Close the browser window to trigger profile shutdown
  new_browser->GetWindow()->Close();

  // Wait for profile to shut down.
  profile_shutdown_waiter.Wait();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Verifies that programmatically uninstalling an extension followed immediately
// by a request from WebUI to disable the extension returns early without
// crashing.
IN_PROC_BROWSER_TEST_F(ExtensionServiceBrowserTest,
                       DisableUninstalledExtensionFromWebUI) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("good.crx"));
  ASSERT_TRUE(extension);
  const ExtensionId extension_id = extension->id();
  ASSERT_TRUE(
      extension_registry()->enabled_extensions().Contains(extension_id));

  // Programmatically uninstall the extension and verify it is removed from
  // `ExtensionRegistry`.
  UninstallExtension(extension_id);
  ASSERT_FALSE(extension_registry()->GetExtensionById(
      extension_id, ExtensionRegistry::EVERYTHING));

  // Navigate to chrome://settings to ensure ExtensionControlHandler is
  // registered.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings")));

  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  content::WebUI* web_ui = web_contents->GetWebUI();
  ASSERT_TRUE(web_ui);

  // Add a helper handler to synchronize WebUI messages. Since WebUI messages
  // are processed in order on the UI thread, sending 'quit' after
  // 'disableExtension' guarantees that 'disableExtension' has been processed
  // by the time 'quit' is handled.
  base::RunLoop web_ui_disable_extension_processed_run_loop;
  web_ui->AddMessageHandler(std::make_unique<WebUIQuitHandler>(
      web_ui_disable_extension_processed_run_loop.QuitClosure()));

  // Trigger the disable request as-if from WebUI, followed by 'quit' to
  // synchronize.
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      base::StringPrintf("chrome.send('disableExtension', ['%s']);"
                         "chrome.send('quit');",
                         extension_id.c_str())));

  {
    SCOPED_TRACE(
        "waiting for disableExtension web UI message to be received and "
        "processed");
    web_ui_disable_extension_processed_run_loop.Run();
  }

  // Verify that the extension remains uninstalled and we didn't crash.
  EXPECT_FALSE(extension_registry()->GetExtensionById(
      extension_id, ExtensionRegistry::EVERYTHING));

  // Verify that we didn't write disable reasons to prefs.
  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->GetDisableReasons(extension_id).empty());
}

}  // namespace extensions
