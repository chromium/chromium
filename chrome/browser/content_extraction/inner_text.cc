// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_extraction/inner_text.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/content_extraction/inner_text.mojom.h"

namespace content_extraction {

using Segments = std::vector<blink::mojom::InnerTextSegmentPtr>;

namespace {

// Returns the number of bytes needed for the combined text.
size_t CalculateTotalStringSize(const blink::mojom::InnerTextFrame& frame) {
  size_t size = 0;
  for (auto& segment : frame.segments) {
    if (segment->is_text()) {
      size += segment->get_text().size();
    } else if (segment->is_frame()) {
      size += CalculateTotalStringSize(*segment->get_frame());
    }
  }
  return size;
}

// Appends the text segments to `result.inner_text` as well as setting
// the node offset.
void AppendFrameSegments(const blink::mojom::InnerTextFrame& frame,
                         InnerTextResult& result) {
  for (const auto& segment : frame.segments) {
    if (segment->is_text()) {
      result.inner_text.append(segment->get_text());
    } else if (segment->is_node_location()) {
      result.node_offset = result.inner_text.size();
    } else {
      AppendFrameSegments(*segment->get_frame(), result);
    }
  }
}

void OnGotInnerText(base::TimeTicks start_time,
                    mojo::Remote<blink::mojom::InnerTextAgent> remote_interface,
                    InnerTextCallback callback,
                    blink::mojom::InnerTextFramePtr mojo_frame) {
  std::unique_ptr<InnerTextResult> result;
  if (internal::IsInnerTextFrameValid(mojo_frame)) {
    result = internal::CreateInnerTextResult(*mojo_frame);
    const base::TimeDelta total_time = base::TimeTicks::Now() - start_time;
    base::UmaHistogramTimes("ContentExtraction.InnerText.Time", total_time);
    base::UmaHistogramCounts10M("ContentExtraction.InnerText.Size",
                                result->inner_text.size());
  }
  base::UmaHistogramBoolean("ContentExtraction.InnerText.ValidResults",
                            result != nullptr);
  std::move(callback).Run(std::move(result));
}

}  // namespace

void GetInnerText(content::RenderFrameHost& host,
                  std::optional<int> node_id,
                  InnerTextCallback callback) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  mojo::Remote<blink::mojom::InnerTextAgent> agent;
  host.GetRemoteInterfaces()->GetInterface(agent.BindNewPipeAndPassReceiver());
  auto params = blink::mojom::InnerTextParams::New();
  if (node_id) {
    params->node_id = *node_id;
  }
  auto* agent_ptr = agent.get();
  agent_ptr->GetInnerText(
      std::move(params),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&OnGotInnerText, start_time, std::move(agent),
                         std::move(callback)),
          nullptr));
}

namespace internal {

bool IsInnerTextFrameValid(const blink::mojom::InnerTextFramePtr& frame) {
  if (!frame) {
    return false;
  }
  for (auto& segment : frame->segments) {
    if (!segment ||
        (!segment->is_text() && !segment->is_frame() &&
         !segment->is_node_location()) ||
        (segment->is_frame() && !IsInnerTextFrameValid(segment->get_frame()))) {
      return false;
    }
  }
  return true;
}

std::unique_ptr<InnerTextResult> CreateInnerTextResult(
    const blink::mojom::InnerTextFrame& frame) {
  std::unique_ptr<InnerTextResult> result = std::make_unique<InnerTextResult>();
  // Have the string reserve enough space for all the text.
  result->inner_text.reserve(CalculateTotalStringSize(frame));
  AppendFrameSegments(frame, *result);
  return result;
}

}  // namespace internal
}  // namespace content_extraction
