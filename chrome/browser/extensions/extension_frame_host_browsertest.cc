// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_frame_host.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/chrome_extension_host_delegate.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/user_script_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/test_extensions_browser_client.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

class TestExtensionFrameHost : public ExtensionFrameHost {
 public:
  explicit TestExtensionFrameHost(content::WebContents* web_contents)
      : ExtensionFrameHost(web_contents) {}
  TestExtensionFrameHost(const TestExtensionFrameHost&) = delete;
  TestExtensionFrameHost& operator=(const TestExtensionFrameHost&) = delete;
  ~TestExtensionFrameHost() override = default;

  void SetInvalidRequest(const std::string& name) { invalid_request_ = name; }

 private:
  // mojom::LocalFrameHost:
  void Request(mojom::RequestParamsPtr params,
               RequestCallback callback) override {
    // If the name of |params| is set to an invalid request, it sets it to
    // an empty string so that the request causes an error.
    if (invalid_request_ == params->name) {
      params->name = std::string();
    }
    ExtensionFrameHost::Request(std::move(params), std::move(callback));
  }

  std::string invalid_request_;
};

class TestExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<TestExtensionWebContentsObserver> {
 public:
  TestExtensionWebContentsObserver(const TestExtensionWebContentsObserver&) =
      delete;
  TestExtensionWebContentsObserver& operator=(
      const TestExtensionWebContentsObserver&) = delete;
  ~TestExtensionWebContentsObserver() override = default;

  // Creates and initializes an instance of this class for the given
  // |web_contents|, if it doesn't already exist.
  static void CreateForWebContents(content::WebContents* web_contents) {
    content::WebContentsUserData<
        TestExtensionWebContentsObserver>::CreateForWebContents(web_contents);
    // Initialize this instance if necessary.
    FromWebContents(web_contents)->Initialize();
  }

  // Overrides to create TestExtensionFrameHost.
  std::unique_ptr<ExtensionFrameHost> CreateExtensionFrameHost(
      content::WebContents* web_contents) override {
    return std::make_unique<TestExtensionFrameHost>(web_contents);
  }

 private:
  friend class content::WebContentsUserData<TestExtensionWebContentsObserver>;

  explicit TestExtensionWebContentsObserver(content::WebContents* web_contents)
      : ExtensionWebContentsObserver(web_contents),
        content::WebContentsUserData<TestExtensionWebContentsObserver>(
            *web_contents) {}

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TestExtensionWebContentsObserver);

class TestExtensionHostDelegate : public ChromeExtensionHostDelegate {
 public:
  TestExtensionHostDelegate() = default;
  TestExtensionHostDelegate(const TestExtensionHostDelegate&) = delete;
  TestExtensionHostDelegate& operator=(const TestExtensionHostDelegate&) =
      delete;
  ~TestExtensionHostDelegate() override = default;

  // Overrides to create TestExtensionWebContentsObserver.
  void OnExtensionHostCreated(content::WebContents* web_contents) override {
    TestExtensionWebContentsObserver::CreateForWebContents(web_contents);
  }
};

// This test does not use ChromeExtensionsBrowserClient because it has strict
// initialization and shutdown timing that is difficult to override in a test
// (e.g. init occurs in the middle of g_browser_process initialization).
class ExtensionFrameHostTestExtensionsBrowserClient
    : public TestExtensionsBrowserClient {
 public:
  ExtensionFrameHostTestExtensionsBrowserClient() = default;
  ExtensionFrameHostTestExtensionsBrowserClient(
      const ExtensionFrameHostTestExtensionsBrowserClient&) = delete;
  ExtensionFrameHostTestExtensionsBrowserClient& operator=(
      const ExtensionFrameHostTestExtensionsBrowserClient&) = delete;
  ~ExtensionFrameHostTestExtensionsBrowserClient() override = default;

  // Overrides to create TestExtensionHostDelegate.
  std::unique_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate()
      override {
    return std::make_unique<TestExtensionHostDelegate>();
  }

  // Overrides to return TestExtensionWebContentsObserver.
  ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) override {
    return TestExtensionWebContentsObserver::FromWebContents(web_contents);
  }
};

}  // namespace

class ExtensionFrameHostBrowserTest : public ExtensionApiTest {
 public:
  ExtensionFrameHostBrowserTest() = default;
  ExtensionFrameHostBrowserTest(const ExtensionFrameHostBrowserTest&) = delete;
  ExtensionFrameHostBrowserTest& operator=(
      const ExtensionFrameHostBrowserTest&) = delete;
  ~ExtensionFrameHostBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // NOTE: The TestExtensionsBrowserClient needs sufficient dependencies
    // initialized to load and run an extension. If this test suite fails, and
    // you changed ExtensionsBrowserClient recently, you may need to add a
    // more detailed stub method to TestExtensionsBrowserClient.
    user_script_listener_ = std::make_unique<UserScriptListener>();
    extensions_browser_client_ =
        std::make_unique<ExtensionFrameHostTestExtensionsBrowserClient>();
    extensions_browser_client_->SetMainContext(GetProfile());
    extensions_browser_client_->set_user_script_listener(
        user_script_listener_.get());
    extensions_browser_client_->set_extension_system_factory(
        ChromeExtensionSystemFactory::GetInstance());

    old_extensions_browser_client_ = ExtensionsBrowserClient::Get();
    ExtensionsBrowserClient::Set(extensions_browser_client_.get());
  }

  void TearDownOnMainThread() override {
    // Avoid dangling pointers.
    extensions_browser_client_->set_extension_system_factory(nullptr);
    extensions_browser_client_->set_user_script_listener(nullptr);
    user_script_listener_.reset();

    ExtensionsBrowserClient::Set(old_extensions_browser_client_);
    old_extensions_browser_client_ = nullptr;
    extensions_browser_client_.reset();

    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  const Extension* extension() const { return extension_.get(); }

  // Loads and runs the test extension. Called from the test body to ensure that
  // test suite setup is complete before the extension is loaded.
  void LoadTestExtension() {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII("extensions");

    ResultCatcher catcher;
    extension_ = LoadExtension(test_data_dir.AppendASCII("extension"));
    ASSERT_TRUE(extension_.get());
    ASSERT_TRUE(catcher.GetNextResult());
  }

  void SetInvalidNameOnRequest(const std::string& method_name) {
    ExtensionHost* host =
        ProcessManager::Get(GetProfile())
            ->GetBackgroundHostForExtension(extension()->id());
    ASSERT_TRUE(host);
    ASSERT_TRUE(host->host_contents());
    ExtensionWebContentsObserver* observer =
        extensions_browser_client_->GetExtensionWebContentsObserver(
            host->host_contents());
    ASSERT_TRUE(observer);
    auto* efh = observer->extension_frame_host_for_testing();
    ASSERT_TRUE(efh);
    static_cast<TestExtensionFrameHost*>(efh)->SetInvalidRequest(method_name);
  }

 private:
  scoped_refptr<const Extension> extension_;
  std::unique_ptr<UserScriptListener> user_script_listener_;
  raw_ptr<ExtensionsBrowserClient> old_extensions_browser_client_ = nullptr;
  std::unique_ptr<ExtensionFrameHostTestExtensionsBrowserClient>
      extensions_browser_client_;
};

// Test that when ExtensionFrameHost dispatches an invalid request it gets
// an error associated with it. This is a regression test for
// https://crbug.com/40759577.
// NOTE: If this test fails or crashes, and you changed ExtensionBrowserClient
// recently, see the note in SetUpOnMainThread() above.
IN_PROC_BROWSER_TEST_F(ExtensionFrameHostBrowserTest, InvalidNameRequest) {
  // This needs to be done after the profile is created.
  LoadTestExtension();
  // Set 'test.getConfig' is invalid request.
  SetInvalidNameOnRequest("test.getConfig");
  // Run a script asynchronously that passes the test.
  ResultCatcher catcher;
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      GetProfile(), extension()->id(), R"(
        chrome.test.getConfig(() => {
          const expectedError = 'Access to extension API denied.';
          if (chrome.runtime.lastError &&
            expectedError === chrome.runtime.lastError.message) {
            chrome.test.notifyPass();
          } else {
            chrome.test.notifyFail('Test Failed');
          }
        });)"));

  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace extensions
