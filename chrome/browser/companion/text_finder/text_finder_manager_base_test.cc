// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/text_finder/text_finder_manager_base_test.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion {

namespace {

constexpr char kBaseUrl[] = "https://www.example.com/";

}  // namespace

MockAnnotationAgentContainer::MockAnnotationAgentContainer() = default;

MockAnnotationAgentContainer::~MockAnnotationAgentContainer() = default;

void MockAnnotationAgentContainer::MockCreateAgent(
    mojo::PendingRemote<blink::mojom::AnnotationAgentHost> host_remote,
    mojo::PendingReceiver<blink::mojom::AnnotationAgent> agent_receiver,
    blink::mojom::AnnotationType type,
    const std::string& serialized_selector) {
  return;
}

void MockAnnotationAgentContainer::Bind(mojo::ScopedMessagePipeHandle handle) {
  is_bound_ = true;
  receiver_.Bind(mojo::PendingReceiver<blink::mojom::AnnotationAgentContainer>(
      std::move(handle)));
}

TextFinderManagerBaseTest::TextFinderManagerBaseTest() = default;

TextFinderManagerBaseTest::~TextFinderManagerBaseTest() = default;

void TextFinderManagerBaseTest::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
}

void TextFinderManagerBaseTest::TearDown() {
  // Owned web contentses must be destroyed before the test harness.
  web_contents_list_.clear();
  content::RenderViewHostTestHarness::TearDown();
}

std::unique_ptr<content::BrowserContext>
TextFinderManagerBaseTest::CreateBrowserContext() {
  return std::make_unique<TestingProfile>();
}

TextFinderManager* TextFinderManagerBaseTest::CreateTextFinderManagerForTest(
    MockAnnotationAgentContainer* mock_container) {
  // Create a test frame and navigate it to a unique URL.
  std::unique_ptr<content::WebContents> wc = CreateTestWebContents();
  content::RenderFrameHostTester::For(wc->GetPrimaryMainFrame())
      ->InitializeRenderFrameIfNeeded();
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      wc.get(),
      GURL(kBaseUrl + base::NumberToString(web_contents_list_.size())));

  std::unique_ptr<service_manager::InterfaceProvider::TestApi> test_api;
  if (mock_container) {
    CHECK(!mock_container->is_bound());
    test_api = std::make_unique<service_manager::InterfaceProvider::TestApi>(
        wc->GetPrimaryMainFrame()->GetRemoteInterfaces());
    test_api->SetBinderForName(
        blink::mojom::AnnotationAgentContainer::Name_,
        base::BindRepeating(&MockAnnotationAgentContainer::Bind,
                            base::Unretained(mock_container)));
  }

  // Create and attach a `TextFinderManager` to the primary page.
  content::Page& page = wc->GetPrimaryPage();
  TextFinderManager::CreateForPage(page);
  TextFinderManager* text_finder_manager = TextFinderManager::GetForPage(page);
  DCHECK(text_finder_manager);
  web_contents_list_.emplace_back(std::move(wc));

  CHECK(!mock_container || mock_container->is_bound());

  return text_finder_manager;
}

}  // namespace companion
