// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.offline_items_collection.OfflineItem;

/**
 * A {@link OfflineItemFilter} responsible for pruning out off the record items if we are not
 * showing them for this instance of the download manager.
 */
@NullMarked
public class OffTheRecordOfflineItemFilter extends OfflineItemFilter {
    private final boolean mIncludeOffTheRecordItems;

    /** Creates an instance of this filter and wraps {@code source}. */
    public OffTheRecordOfflineItemFilter(boolean offTheRecord, OfflineItemFilterSource source) {
        super(source);
        mIncludeOffTheRecordItems = offTheRecord;
        onFilterChanged();
    }

    // OfflineItemFilter implementation.
    @Override
    protected boolean isFilteredOut(OfflineItem item) {
        // Always show downloads from regular mode.
        if (!item.isOffTheRecord) return false;

        try {
            // Only show downloads from primary OTR profile if mIncludeOffTheRecordItems is true.
            // TODO(crbug.com/40254448): Pass the owning profile instead of using the last used
            // regular profile.
            OtrProfileId profileId =
                    OtrProfileId.deserializeAndVerify(
                            assumeNonNull(item.otrProfileId),
                            ProfileManager.getLastUsedRegularProfile());
            boolean isPrimaryOtr = assumeNonNull(profileId).isPrimaryOtrId();
            return !(mIncludeOffTheRecordItems && isPrimaryOtr);
        } catch (IllegalStateException e) {
            return true;
        }
    }
}
