// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client_parts.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/mojom/echo.test-mojom.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

namespace {

using ::testing::_;
using ::testing::WithArg;

// Only mocks methods that are tested.
class MockChromeContentBrowserClientParts
    : public ChromeContentBrowserClientParts {
 public:
  MOCK_METHOD(void,
              ExposeInterfacesToRenderer,
              (service_manager::BinderRegistry*,
               blink::AssociatedInterfaceRegistry*,
               content::RenderProcessHost*),
              (override));
  MOCK_METHOD(void,
              ExposeInterfacesToChild,
              (mojo::BinderMapWithContext<content::BrowserChildProcessHost*>*),
              (override));
};

// Echo implementation for test that adds a suffix to the echoed string.
class TestForEcho : public test::mojom::Echo {
 public:
  explicit TestForEcho(std::string_view suffix) : suffix_(suffix) {}

  void EchoString(const std::string& input,
                  EchoStringCallback callback) override {
    std::move(callback).Run(base::StrCat({input, ".", suffix_}));
  }

  static void BindPipe(std::string_view context,
                       mojo::ScopedMessagePipeHandle pipe) {
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<TestForEcho>(context),
        mojo::PendingReceiver<test::mojom::Echo>(std::move(pipe)));
  }

  static void BindReceiver(std::string_view context,
                           content::BrowserChildProcessHost* unused,
                           mojo::PendingReceiver<test::mojom::Echo> receiver) {
    mojo::MakeSelfOwnedReceiver(std::make_unique<TestForEcho>(context),
                                std::move(receiver));
  }

 private:
  std::string suffix_;
};

class ChromeContentBrowserClientPartsTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Ensure that a ContentBrowserClient is registered.
    ASSERT_TRUE(content::GetContentClientForTesting());
    ASSERT_TRUE(content::GetContentClientForTesting()->browser());
  }

  // Adds a new MockChromeContentBrowserClientParts to the ContentBrowserClient,
  // and returns a pointer to it.
  MockChromeContentBrowserClientParts* AddMockClientParts() {
    auto* browser_client = static_cast<ChromeContentBrowserClient*>(
        content::GetContentClientForTesting()->browser());
    auto client_parts = std::make_unique<MockChromeContentBrowserClientParts>();
    auto* mock_client_parts = client_parts.get();
    browser_client->AddExtraPartForTesting(std::move(client_parts));
    return mock_client_parts;
  }
};

TEST_F(ChromeContentBrowserClientPartsTest, ExposeInterfaces) {
  auto* mock_client_parts1 = AddMockClientParts();
  auto* mock_client_parts2 = AddMockClientParts();

  // ExposeInterfaces calls should be passed through to all the parts.
  service_manager::BinderRegistry binder_registry;
  blink::AssociatedInterfaceRegistry associated_interface_registry;
  EXPECT_CALL(*mock_client_parts1,
              ExposeInterfacesToRenderer(
                  &binder_registry, &associated_interface_registry, process()));
  EXPECT_CALL(*mock_client_parts1, ExposeInterfacesToChild(_))
      .WillOnce(
          [](mojo::BinderMapWithContext<content::BrowserChildProcessHost*>*
                 map) {
            map->Add<test::mojom::Echo>(
                base::BindRepeating(&TestForEcho::BindReceiver, "BCPH"));
          });

  EXPECT_CALL(*mock_client_parts2,
              ExposeInterfacesToRenderer(
                  &binder_registry, &associated_interface_registry, process()))
      .WillOnce(WithArg<2>([](content::RenderProcessHost* rph) {
        static_cast<content::MockRenderProcessHost*>(rph)
            ->OverrideBinderForTesting(
                test::mojom::Echo::Name_,
                base::BindRepeating(&TestForEcho::BindPipe, "RPH"));
      }));
  EXPECT_CALL(*mock_client_parts2, ExposeInterfacesToChild(_));

  // Unlike BrowserChildProcessHostImpl and RenderProcessHostImpl,
  // MockRenderProcessHost doesn't call ExposeInterfacesToRenderer
  // automatically.
  content::GetContentClientForTesting()->browser()->ExposeInterfacesToRenderer(
      &binder_registry, &associated_interface_registry, process());

  content::BrowserChildProcessHostDelegate dummy_delegate;
  auto bcph = content::BrowserChildProcessHost::Create(
      content::PROCESS_TYPE_UTILITY, &dummy_delegate,
      content::ChildProcessHost::IpcMode::kNormal);

  // Ensure the exposed interfaces are registered correctly by expecting each to
  // append a suffix to the echoed string.
  mojo::Remote<test::mojom::Echo> echo_remote;
  process()->BindReceiver(echo_remote.BindNewPipeAndPassReceiver());
  echo_remote->EchoString("foo", base::BindOnce([](const std::string& output) {
                            EXPECT_EQ(output, "foo.RPH");
                          }));
  echo_remote.reset();
  bcph->GetHost()->BindReceiver(echo_remote.BindNewPipeAndPassReceiver());
  echo_remote->EchoString("bar", base::BindOnce([](const std::string& output) {
                            EXPECT_EQ(output, "bar.BCPH");
                          }));
}

}  // namespace
