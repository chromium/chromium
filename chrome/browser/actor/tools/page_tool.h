// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "content/public/browser/weak_document_ptr.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrameHost;
}

namespace actor {

class AggregatedJournal;
class RenderFrameChangeObserver;

// A page tool is any tool implemented in the renderer by ToolExecutor. This
// class is shared by multiple tools and serves to implement the mojo shuttling
// of the request to the renderer.
class PageTool : public Tool {
 public:
  PageTool(AggregatedJournal& journal,
           content::RenderFrameHost& frame,
           const optimization_guide::proto::Action& action);
  ~PageTool() override;

  // actor::Tool
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;

 private:
  void FinishInvoke(mojom::ActionResultPtr result);

  void PostFinishInvoke(mojom::ActionResultCode result_code);

  InvokeCallback invoke_callback_;
  content::WeakDocumentPtr render_frame_host_;
  std::unique_ptr<RenderFrameChangeObserver> frame_change_observer_;
  optimization_guide::proto::Action action_;
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame_;

  base::WeakPtrFactory<PageTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_H_
