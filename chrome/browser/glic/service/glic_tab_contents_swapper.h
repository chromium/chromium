// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CONTENTS_SWAPPER_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CONTENTS_SWAPPER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/supports_user_data.h"
#include "components/tab_groups/tab_group_id.h"

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicPlaceholderUserData : public base::SupportsUserData::Data {
 public:
  static constexpr char kKey[] = "glic_placeholder_user_data";
};

// Swaps the active Glic tab's WebContents with a placeholder WebContents.
// Reclaims the Glic WebContents by invoking the `reclaim_callback`.
// Returns the newly inserted placeholder tab, or nullptr on failure.
tabs::TabInterface* SwapGlicTabToPlaceholder(
    tabs::TabInterface* real_tab,
    base::OnceCallback<void(std::unique_ptr<content::WebContents>)>
        reclaim_callback);

// Swaps the placeholder tab's WebContents back with the real Glic WebContents.
// Returns the newly created Glic tab, or nullptr on failure.
tabs::TabInterface* SwapPlaceholderToGlic(
    tabs::TabInterface* placeholder_tab,
    std::unique_ptr<content::WebContents> real_contents,
    std::optional<tab_groups::TabGroupId> tab_group_id);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_TAB_CONTENTS_SWAPPER_H_
