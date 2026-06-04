// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/actions_util.h"

#include <string_view>

#include "ui/gfx/text_utils.h"

namespace chrome {

std::u16string GetCleanTitleAndTooltipText(std::u16string string) {
  static constexpr std::u16string_view kEllipsisUnicode{u"\u2026"};
  static constexpr std::u16string_view kEllipsisText{u"..."};

  const auto remove_ellipsis = [&string](const std::u16string_view ellipsis) {
    const size_t ellipsis_pos = string.find(ellipsis);
    if (ellipsis_pos != std::u16string::npos) {
      string.erase(ellipsis_pos);
    }
  };
  remove_ellipsis(kEllipsisUnicode);
  remove_ellipsis(kEllipsisText);
  return gfx::RemoveAccelerator(string);
}

}  // namespace chrome
