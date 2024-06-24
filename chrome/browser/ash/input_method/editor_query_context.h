// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_QUERY_CONTEXT_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_QUERY_CONTEXT_H_

#include <optional>
#include <string>
#include <string_view>

namespace ash::input_method {

struct EditorQueryContext {
  EditorQueryContext(std::optional<std::string_view> preset_query_id,
                     std::optional<std::string_view> freeform_text);
  EditorQueryContext(const EditorQueryContext& other);

  ~EditorQueryContext();

  std::optional<std::string> preset_query_id;
  std::optional<std::string> freeform_text;
};

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_QUERY_CONTEXT_H_
