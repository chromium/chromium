// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_context.h"

#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "ui/gfx/text_elider.h"

namespace glic {

namespace {

// We send a truncated version of the selection to the panel as it does not
// need to be the full text.
constexpr size_t kMaxSelectionLengthSentToPanel = 100;

}  // namespace

mojom::AdditionalContextPtr CreateTextSelectionContext(
    content::WebContents* web_contents,
    const std::u16string& selected_text,
    SelectionTextLength text_length) {
  auto context = mojom::AdditionalContext::New();
  context->source = mojom::AdditionalContextSource::kTextSelection;
  std::vector<mojom::AdditionalContextPartPtr> parts;
  if (!selected_text.empty()) {
    auto context_data = mojom::ContextData::New();
    context_data->mime_type = kMimeTypeGlicSelection;
    std::u16string text = selected_text;
    if (text_length == SelectionTextLength::kElided) {
      gfx::ElideString(selected_text, kMaxSelectionLengthSentToPanel, &text);
    }
    std::string utf8_text = base::UTF16ToUTF8(text);
    context_data->data =
        mojo_base::BigBuffer(base::as_bytes(base::span(utf8_text)));
    parts.push_back(
        mojom::AdditionalContextPart::NewData(std::move(context_data)));
  }
  if (auto* tab_interface =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    context->tab_id = tab_interface->GetHandle().raw_value();
  }
  context->parts = std::move(parts);
  return context;
}

mojom::AdditionalContextPtr CreateRegionSelectionContext(
    const tabs::TabInterface& tab,
    mojom::TabContextResultPtr tab_context,
    CapturedRegions regions) {
  auto context = mojom::AdditionalContext::New();
  std::vector<mojom::AdditionalContextPartPtr> parts;
  parts.push_back(
      mojom::AdditionalContextPart::NewTabContext(std::move(tab_context)));
  for (auto& region : regions) {
    parts.push_back(mojom::AdditionalContextPart::NewPendingRegion(
        mojom::PendingCapturedRegion::New(region.first,
                                          std::move(region.second))));
  }
  context->source = mojom::AdditionalContextSource::kRegionSelection;
  context->tab_id = tab.GetHandle().raw_value();
  context->parts = std::move(parts);
  return context;
}

}  // namespace glic
