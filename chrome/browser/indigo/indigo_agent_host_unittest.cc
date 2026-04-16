// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_agent_host.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/component_updater/indigo_component_installer.h"
#include "chrome/common/indigo/indigo.mojom.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/page.h"
#include "content/public/test/navigation_simulator.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace indigo {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

constexpr std::string_view kScriptFilename = "test_script.js";
constexpr std::string_view kScriptContent = "console.log('test');";

class MockIndigoAgent : public chrome::mojom::IndigoAgent {
 public:
  MockIndigoAgent() = default;
  ~MockIndigoAgent() override = default;

  MOCK_METHOD(void,
              InjectScript,
              (const std::string&,
               const GURL&,
               const url::Origin&,
               mojo::PendingAssociatedRemote<chrome::mojom::IndigoAgentHost>,
               InjectScriptCallback),
              (override));
  MOCK_METHOD(void, Invoke, (InvokeCallback), (override));

  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this,
                   mojo::PendingAssociatedReceiver<chrome::mojom::IndigoAgent>(
                       std::move(handle)));
  }

 private:
  mojo::AssociatedReceiverSet<chrome::mojom::IndigoAgent> receivers_;
};

class IndigoAgentHostTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    script_path_ = temp_dir_.GetPath().AppendASCII(kScriptFilename);
    ASSERT_TRUE(base::WriteFile(script_path_, kScriptContent));
  }

  void TearDown() override {
    component_updater::ResetIndigoInstallDirForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void OverrideAgentBinder() {
    main_rfh()->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        chrome::mojom::IndigoAgent::Name_,
        base::BindRepeating(&MockIndigoAgent::Bind,
                            base::Unretained(&mock_agent_)));
  }

  void SetIndigoScriptSwitch(const base::FilePath& path) {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchPath(
        "indigo-script", path);
  }

  base::test::ScopedCommandLine scoped_command_line_;
  base::ScopedTempDir temp_dir_;
  base::FilePath script_path_;
  MockIndigoAgent mock_agent_;
};

TEST_F(IndigoAgentHostTest, AutomaticInjectionOnFirstInvoke) {
  SetIndigoScriptSwitch(script_path_);
  NavigateAndCommit(GURL("https://example.com"));
  OverrideAgentBinder();
  content::Page& page = main_rfh()->GetPage();
  IndigoAgentHost* host = IndigoAgentHost::GetOrCreateForPage(page);

  const GURL expected_script_url = net::FilePathToFileURL(script_path_);
  EXPECT_CALL(mock_agent_,
              InjectScript(std::string(kScriptContent), expected_script_url,
                           testing::Property(&url::Origin::opaque, true), _, _))
      .WillOnce(RunOnceCallback<4>());

  base::test::TestFuture<void> invoke_future;
  EXPECT_CALL(mock_agent_, Invoke(_))
      .WillOnce([&](chrome::mojom::IndigoAgent::InvokeCallback callback) {
        std::move(callback).Run();
        invoke_future.SetValue();
      });

  EXPECT_TRUE(host->Invoke());
  EXPECT_TRUE(invoke_future.Wait());
}

TEST_F(IndigoAgentHostTest, MultipleInvokesDuringInjectionAreQueued) {
  SetIndigoScriptSwitch(script_path_);
  NavigateAndCommit(GURL("https://example.com"));
  OverrideAgentBinder();
  content::Page& page = main_rfh()->GetPage();
  IndigoAgentHost* host = IndigoAgentHost::GetOrCreateForPage(page);

  chrome::mojom::IndigoAgent::InjectScriptCallback saved_inject_callback;
  base::test::TestFuture<void> inject_called_future;
  EXPECT_CALL(mock_agent_, InjectScript(_, _, _, _, _))
      .WillOnce(
          [&](const std::string&, const GURL&, const url::Origin&,
              mojo::PendingAssociatedRemote<chrome::mojom::IndigoAgentHost>,
              chrome::mojom::IndigoAgent::InjectScriptCallback callback) {
            saved_inject_callback = std::move(callback);
            inject_called_future.SetValue();
          });

  base::test::TestFuture<void> invokes_future;
  // Set up expectation for Invoke calls BEFORE they are dispatched.
  EXPECT_CALL(mock_agent_, Invoke(_))
      .WillOnce(RunOnceCallback<0>())
      .WillOnce([&](chrome::mojom::IndigoAgent::InvokeCallback callback) {
        std::move(callback).Run();
        invokes_future.SetValue();
      });

  // First invoke starts injection.
  EXPECT_TRUE(host->Invoke());
  // Second invoke should be queued.
  EXPECT_TRUE(host->Invoke());

  // Wait for script loading to happen and InjectScript to be called.
  EXPECT_TRUE(inject_called_future.Wait());

  ASSERT_TRUE(saved_inject_callback);
  std::move(saved_inject_callback).Run();

  // Wait for both Invoke calls to finish.
  EXPECT_TRUE(invokes_future.Wait());
}

TEST_F(IndigoAgentHostTest, StateIsClearedOnCrossDocumentNavigation) {
  SetIndigoScriptSwitch(script_path_);
  NavigateAndCommit(GURL("https://example.com/1"));
  OverrideAgentBinder();
  content::Page& page1 = main_rfh()->GetPage();
  IndigoAgentHost* host1 = IndigoAgentHost::GetOrCreateForPage(page1);

  EXPECT_CALL(mock_agent_, InjectScript(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>());

  base::test::TestFuture<void> invoke1_future;
  EXPECT_CALL(mock_agent_, Invoke(_))
      .WillOnce([&](chrome::mojom::IndigoAgent::InvokeCallback callback) {
        std::move(callback).Run();
        invoke1_future.SetValue();
      });

  host1->Invoke();
  EXPECT_TRUE(invoke1_future.Wait());

  // Navigate to a new page.
  NavigateAndCommit(GURL("https://example.com/2"));
  OverrideAgentBinder();
  content::Page& page2 = main_rfh()->GetPage();
  IndigoAgentHost* host2 = IndigoAgentHost::GetOrCreateForPage(page2);

  EXPECT_NE(host1, host2);

  // New page should trigger injection again.
  EXPECT_CALL(mock_agent_, InjectScript(_, _, _, _, _))
      .WillOnce(RunOnceCallback<4>());

  base::test::TestFuture<void> invoke2_future;
  EXPECT_CALL(mock_agent_, Invoke(_))
      .WillOnce([&](chrome::mojom::IndigoAgent::InvokeCallback callback) {
        std::move(callback).Run();
        invoke2_future.SetValue();
      });

  host2->Invoke();
  EXPECT_TRUE(invoke2_future.Wait());
}

TEST_F(IndigoAgentHostTest, LoadFromInstalledComponent) {
  NavigateAndCommit(GURL("https://example.com"));
  OverrideAgentBinder();
  content::Page& page = main_rfh()->GetPage();
  IndigoAgentHost* host = IndigoAgentHost::GetOrCreateForPage(page);

  base::FilePath component_dir = temp_dir_.GetPath().AppendASCII("component");
  ASSERT_TRUE(base::CreateDirectory(component_dir));
  base::FilePath script_path = component_dir.AppendASCII("content_script.js");
  ASSERT_TRUE(base::WriteFile(script_path, kScriptContent));

  component_updater::IndigoComponentInstallerPolicy policy;
  policy.ComponentReady(base::Version("1.0"), component_dir, base::DictValue());

  const GURL expected_script_url = net::FilePathToFileURL(script_path);
  EXPECT_CALL(mock_agent_,
              InjectScript(std::string(kScriptContent), expected_script_url,
                           testing::Property(&url::Origin::opaque, true), _, _))
      .WillOnce(RunOnceCallback<4>());

  base::test::TestFuture<void> invoke_future;
  EXPECT_CALL(mock_agent_, Invoke(_))
      .WillOnce([&](chrome::mojom::IndigoAgent::InvokeCallback callback) {
        std::move(callback).Run();
        invoke_future.SetValue();
      });

  EXPECT_TRUE(host->Invoke());
  EXPECT_TRUE(invoke_future.Wait());
}

}  // namespace
}  // namespace indigo
