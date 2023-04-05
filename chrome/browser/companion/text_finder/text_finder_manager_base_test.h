// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_MANAGER_BASE_TEST_H_
#define CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_MANAGER_BASE_TEST_H_

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/companion/text_finder/text_finder_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

namespace companion {

// Mock implementation of the renderer's AnnotationAgentContainer interface.
class MockAnnotationAgentContainer
    : public blink::mojom::AnnotationAgentContainer {
 public:
  MockAnnotationAgentContainer();
  ~MockAnnotationAgentContainer() override;

  // blink::mojom::AnnotationAgentContainer
  MOCK_METHOD4(CreateAgent,
               void(mojo::PendingRemote<blink::mojom::AnnotationAgentHost>,
                    mojo::PendingReceiver<blink::mojom::AnnotationAgent>,
                    blink::mojom::AnnotationType,
                    const std::string& /*serialized_selector*/));

  MOCK_METHOD2(CreateAgentFromSelection,
               void(blink::mojom::AnnotationType,
                    CreateAgentFromSelectionCallback));

  void MockCreateAgent(
      mojo::PendingRemote<blink::mojom::AnnotationAgentHost> host_remote,
      mojo::PendingReceiver<blink::mojom::AnnotationAgent> agent_receiver,
      blink::mojom::AnnotationType type,
      const std::string& serialized_selector);

  void Bind(mojo::ScopedMessagePipeHandle handle);

  bool is_bound() const { return is_bound_; }

 private:
  bool is_bound_ = false;
  mojo::Receiver<blink::mojom::AnnotationAgentContainer> receiver_{this};
};

// A base test harness for `TextFinderManager` unit tests. Exposes methods to
// create a new `TextFinderManager`, and attach it to mock pages.
class TextFinderManagerBaseTest : public content::RenderViewHostTestHarness {
 public:
  TextFinderManagerBaseTest();
  ~TextFinderManagerBaseTest() override;

 protected:
  // Implements `content::RenderViewHostTestHarness`.
  void SetUp() override;
  void TearDown() override;
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override;

  // Creates a new TextFinderManager attached to mock pages, and bind its
  // `annotation_agent_container_` to `mock_container`.
  TextFinderManager* CreateTextFinderManagerForTest(
      MockAnnotationAgentContainer* mock_container);

  std::vector<std::unique_ptr<content::WebContents>> web_contents_list_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_MANAGER_BASE_TEST_H_
