// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_CONTENT_UTIL_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_CONTENT_UTIL_H_

#include "components/previews/content/previews_decider.h"
#include "content/public/common/previews_state.h"

namespace content {
class NavigationHandle;
}

namespace data_reduction_proxy {
class DataReductionProxyData;
}

namespace previews {

// This bit mask is all the preview types that are fully decided
// before commit.
static const content::PreviewsState kPreCommitPreviews =
    content::SERVER_LITE_PAGE_ON | content::OFFLINE_PAGE_ON |
    content::LITE_PAGE_REDIRECT_ON;

// Returns whether |previews_state| has any enabled previews.
bool HasEnabledPreviews(content::PreviewsState previews_state);

// Returns the bitmask of enabled client-side previews and the
// current effective network connection given |previews_decider|.
// This handles the mapping of previews::PreviewsType enum values to bitmask
// definitions for content::PreviewsState.
// |previews_triggering_logic_already_ran| is used to prevent offline previews
// from being updated if previews triggering logic has already run.
// |previews_data| is populated with relevant information.
content::PreviewsState DetermineAllowedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    bool previews_triggering_logic_already_ran,
    bool is_data_saver_user,
    previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle);

// If this Chrome session is in a coin flip holdback, possibly modify the
// previews state of the navigation according to a random coin flip. This method
// should only be called before commit (at navigation start or redirect) and
// will only impact previews that are decided before commit.
content::PreviewsState MaybeCoinFlipHoldbackBeforeCommit(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle);

// Returns an updated PreviewsState given |previews_state| that has already
// been updated wrt server previews. This should be called at Navigation Commit
// time. It will defer to any server preview set, otherwise it chooses which
// client preview bits to retain for processing the main frame response.
content::PreviewsState DetermineCommittedClientPreviewsState(
    previews::PreviewsUserData* previews_data,
    const GURL& url,
    content::PreviewsState previews_state,
    const previews::PreviewsDecider* previews_decider,
    content::NavigationHandle* navigation_handle);

// Returns an updated PreviewsState with respect to server previews
// given the main frame's committed |request| and the |initial_state|
// of enabled previews. |data| must have already been updated with
// respect to the main frame response headers.
content::PreviewsState DetermineCommittedServerPreviewsState(
    data_reduction_proxy::DataReductionProxyData* data,
    content::PreviewsState initial_state);

// If this Chrome session is in a coin flip holdback, possibly modify the
// previews state of the navigation according to a random coin flip. This method
// should only be called after commit and may impact all preview types. This
// method assume |MaybeCoinFlipHoldbackBeforeCommit| has already been called
// with the same |navigation_handle|.
content::PreviewsState MaybeCoinFlipHoldbackAfterCommit(
    content::PreviewsState initial_state,
    content::NavigationHandle* navigation_handle);

// Returns the effective PreviewsType known on a main frame basis given the
// |previews_state| bitmask for the committed main frame. This uses the same
// previews precendence consideration as |DetermineCommittedClientPreviewsState|
// in case it is called on a PreviewsState value that has not been filtered
// through that method.
previews::PreviewsType GetMainFramePreviewsType(
    content::PreviewsState previews_state);

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_CONTENT_UTIL_H_
