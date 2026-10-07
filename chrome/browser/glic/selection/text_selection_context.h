// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_TEXT_SELECTION_CONTEXT_H_
#define CHROME_BROWSER_GLIC_SELECTION_TEXT_SELECTION_CONTEXT_H_

#include <string>

#include "chrome/browser/glic/host/glic.mojom-forward.h"

namespace content {
class WebContents;
}  // namespace content

namespace glic {

mojom::AdditionalContextPtr CreateTextSelectionContext(
    content::WebContents* web_contents,
    const std::u16string& selected_text);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_TEXT_SELECTION_CONTEXT_H_
