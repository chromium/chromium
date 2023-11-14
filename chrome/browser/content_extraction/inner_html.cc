// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_extraction/inner_html.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/inner_html.mojom.h"

namespace content_extraction {

namespace {

// Helper to request inner-html and run the real callback (as well as logging
// histograms).
class MojoHandler {
 public:
  ~MojoHandler() {
    if (callback_) {
      RunCallback(std::nullopt);
    }
  }

  static void RequestInnerHtml(content::RenderFrameHost& host,
                               InnerHtmlCallback callback) {
    std::unique_ptr<MojoHandler> handler =
        base::WrapUnique(new MojoHandler(std::move(callback)));
    host.GetRemoteInterfaces()->GetInterface(
        handler->agent_.BindNewPipeAndPassReceiver());
    MojoHandler* handler_ptr = handler.get();
    handler_ptr->agent_->GetInnerHtml(
        base::BindOnce(&MojoHandler::OnGotInnerHtml, std::move(handler)));
  }

 private:
  explicit MojoHandler(InnerHtmlCallback callback)
      : callback_(std::move(callback)) {}

  void OnGotInnerHtml(const std::string& inner_html) {
    const base::TimeDelta total_time = base::TimeTicks::Now() - start_time_;
    base::UmaHistogramTimes("ContentExtraction.InnerHtml.Time", total_time);
    base::UmaHistogramCounts10M("ContentExtraction.InnerHtml.Size",
                                inner_html.size());
    RunCallback(std::move(inner_html));
  }

  void RunCallback(const std::optional<std::string>& inner_html) {
    if (!callback_) {
      return;
    }
    base::UmaHistogramBoolean("ContentExtraction.InnerHtml.ValidResults",
                              inner_html.has_value());
    std::move(callback_).Run(inner_html);
  }

  const base::TimeTicks start_time_ = base::TimeTicks::Now();
  InnerHtmlCallback callback_;
  mojo::Remote<blink::mojom::InnerHtmlAgent> agent_;
};

}  // namespace

void GetInnerHtml(content::RenderFrameHost& host, InnerHtmlCallback callback) {
  MojoHandler::RequestInnerHtml(host, std::move(callback));
}

}  // namespace content_extraction
