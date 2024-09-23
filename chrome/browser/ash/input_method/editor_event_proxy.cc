// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_event_proxy.h"

#include "base/strings/utf_offset_string_conversions.h"

namespace ash::input_method {

namespace {

orca::mojom::ContextPtr CreateContext(const std::u16string& text,
                                      gfx::Range range) {
  std::vector<size_t> offsets = {range.start(), range.end()};
  const std::string text_utf8 =
      base::UTF16ToUTF8AndAdjustOffsets(text, &offsets);

  auto context = orca::mojom::Context::New();
  context->surrounding_text = orca::mojom::SurroundingText::New(
      text_utf8, gfx::Range(offsets[0], offsets[1]));
  return context;
}

}  // namespace

EditorEventProxy::EditorEventProxy(
    mojo::PendingAssociatedRemote<orca::mojom::EditorEventSink> remote)
    : editor_event_sink_remote_(std::move(remote)) {}

EditorEventProxy::~EditorEventProxy() = default;

void EditorEventProxy::OnSurroundingTextChanged(const std::u16string& text,
                                                gfx::Range range) {
  editor_event_sink_remote_->OnContextUpdated(CreateContext(text, range));
}

}  // namespace ash::input_method
