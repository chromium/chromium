// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_tab_context_test_util.h"

#include "base/functional/bind.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"

namespace geic {

class TabContextTestHelper::FakeInnerTextAgent
    : public blink::mojom::InnerTextAgent {
 public:
  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingReceiver<blink::mojom::InnerTextAgent>(std::move(handle)));
  }

  void GetInnerText(blink::mojom::InnerTextParamsPtr params,
                    GetInnerTextCallback callback) override {
    auto frame = blink::mojom::InnerTextFrame::New();
    frame->segments.push_back(blink::mojom::InnerTextSegment::NewText(text_));
    std::move(callback).Run(std::move(frame));
  }

 private:
  mojo::Receiver<blink::mojom::InnerTextAgent> receiver_{this};
  std::string text_ = "Extracted text";
};

class TabContextTestHelper::FakeAIPageContentAgent
    : public blink::mojom::AIPageContentAgent {
 public:
  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.reset();
    receiver_.Bind(mojo::PendingReceiver<blink::mojom::AIPageContentAgent>(
        std::move(handle)));
  }

  void GetAIPageContent(blink::mojom::AIPageContentOptionsPtr options,
                        GetAIPageContentCallback callback) override {
    auto content = blink::mojom::AIPageContent::New();
    content->root_node = blink::mojom::AIPageContentNode::New();
    content->root_node->content_attributes =
        blink::mojom::AIPageContentAttributes::New();
    content->root_node->content_attributes->attribute_type =
        blink::mojom::AIPageContentAttributeType::kRoot;
    content->frame_data = blink::mojom::AIPageContentFrameData::New();
    content->frame_data->frame_interaction_info =
        blink::mojom::AIPageContentFrameInteractionInfo::New();
    content->frame_data->title = "Page Title";
    content->frame_data->default_line_height_px = 16;
    std::move(callback).Run(std::move(content));
  }

  void GetImageBytes(int32_t dom_node_id,
                     GetImageBytesCallback callback) override {
    std::move(callback).Run(nullptr);
  }

 private:
  mojo::Receiver<blink::mojom::AIPageContentAgent> receiver_{this};
};

TabContextTestHelper::TabContextTestHelper(content::WebContents* contents)
    : inner_text_agent_(std::make_unique<FakeInnerTextAgent>()),
      ai_page_content_agent_(std::make_unique<FakeAIPageContentAgent>()) {
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  service_manager::InterfaceProvider::TestApi test_api(
      rfh->GetRemoteInterfaces());
  test_api.SetBinderForName(
      blink::mojom::InnerTextAgent::Name_,
      base::BindRepeating(&FakeInnerTextAgent::Bind,
                          base::Unretained(inner_text_agent_.get())));
  test_api.SetBinderForName(
      blink::mojom::AIPageContentAgent::Name_,
      base::BindRepeating(&FakeAIPageContentAgent::Bind,
                          base::Unretained(ai_page_content_agent_.get())));
}

TabContextTestHelper::~TabContextTestHelper() = default;

}  // namespace geic
