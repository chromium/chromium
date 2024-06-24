// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_query_context.h"

#include <optional>
#include <string_view>

namespace ash::input_method {

EditorQueryContext::EditorQueryContext(
    std::optional<std::string_view> preset_query_id,
    std::optional<std::string_view> freeform_text)
    : preset_query_id(preset_query_id), freeform_text(freeform_text) {}

EditorQueryContext::EditorQueryContext(const EditorQueryContext& other) =
    default;

EditorQueryContext::~EditorQueryContext() = default;

}  // namespace ash::input_method
