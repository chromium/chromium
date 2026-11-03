// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_SELECTION_CONTEXT_H_
#define CHROME_BROWSER_GLIC_SELECTION_SELECTION_CONTEXT_H_

#include <string>
#include <utility>
#include <vector>

#include "base/unguessable_token.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// The MIME type of the selected text in a text selection context.
inline constexpr char kMimeTypeGlicSelection[] = "application/x-glic-selection";

enum class SelectionTextLength {
  // Shortened to 100 characters, which is enough for glic to show it.
  kElided,
  kFull,
};

mojom::AdditionalContextPtr CreateTextSelectionContext(
    content::WebContents* web_contents,
    const std::u16string& selected_text,
    SelectionTextLength text_length);

// Selected regions in the pixels of the tab context's screenshot, keyed by
// region ID.
using CapturedRegions =
    std::vector<std::pair<base::UnguessableToken, mojom::CapturedRegionPtr>>;

// `tab_context` should have the viewport screenshot that `regions` are on.
mojom::AdditionalContextPtr CreateRegionSelectionContext(
    const tabs::TabInterface& tab,
    mojom::TabContextResultPtr tab_context,
    CapturedRegions regions);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_SELECTION_CONTEXT_H_
