// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_IMPORTANT_SITES_UTIL_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_IMPORTANT_SITES_UTIL_H_

#include "components/browsing_data/core/browsing_data_utils.h"
#include "content/public/browser/browsing_data_remover.h"
#include "ui/base/interaction/element_tracker.h"

namespace content {
class BrowsingDataFilterBuilder;
}

namespace browsing_data_important_sites_util {

DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(
    kOpenClearBrowsingDataDialogViaAcceleratorEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kClearBrowsingDataHistoryEventId);
DECLARE_CUSTOM_ELEMENT_EVENT_TYPE(kShowClearBrowsingDataDialogEventId);

// Deletes the types protected by Important Sites with the filter from
// |filter_builder|, the other types are deleted completely.
// |callback| will be called when the deletion finished.
void Remove(uint64_t remove_mask,
            uint64_t origin_mask,
            browsing_data::TimePeriod time_period,
            std::unique_ptr<content::BrowsingDataFilterBuilder> filter_builder,
            content::BrowsingDataRemover* remover,
            base::OnceCallback<void(uint64_t)> callback);

}  // namespace browsing_data_important_sites_util

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_IMPORTANT_SITES_UTIL_H_
