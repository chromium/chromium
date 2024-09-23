// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator responsible for listening to back-end changes affecting the quick delete {@link
 * View}.
 */
class QuickDeleteMediator
        implements QuickDeleteDialogDelegate.TimePeriodChangeObserver,
                QuickDeleteBridge.DomainVisitsCallback {
    private final @NonNull PropertyModel mPropertyModel;
    private final @NonNull Profile mProfile;
    private final @NonNull QuickDeleteBridge mQuickDeleteBridge;
    private final @NonNull QuickDeleteTabsFilter mQuickDeleteRegularTabsFilter;
    // Null when declutter is disabled.
    private final @Nullable QuickDeleteTabsFilter mQuickDeleteArchivedTabsFilter;

    /**
     * @param propertyModel {@link PropertyModel} associated with the quick delete {@link View}.
     * @param profile {@link Profile} to check if the user is signed-in or syncing.
     * @param quickDeleteBridge {@link QuickDeleteBridge} used to fetch the recent visited domain
     *     and site data.
     * @param quickDeleteTabsFilter {@link QuickDeleteTabsFilter} used to fetch the tabs to be
     *     closed data.
     */
    QuickDeleteMediator(
            @NonNull PropertyModel propertyModel,
            @NonNull Profile profile,
            @NonNull QuickDeleteBridge quickDeleteBridge,
            @NonNull QuickDeleteTabsFilter quickDeleteRegularTabsFilter,
            @Nullable QuickDeleteTabsFilter quickDeleteArchivedTabsFilter) {
        mPropertyModel = propertyModel;
        mProfile = profile;
        mQuickDeleteBridge = quickDeleteBridge;
        mQuickDeleteRegularTabsFilter = quickDeleteRegularTabsFilter;
        mQuickDeleteArchivedTabsFilter = quickDeleteArchivedTabsFilter;
    }

    /**
     * A callback which is fired when the user updates the time period spinner inside quick delete.
     *
     * @param timePeriod The latest {@link TimePeriod} in the toggle.
     */
    @Override
    public void onTimePeriodChanged(@TimePeriod int timePeriod) {
        mQuickDeleteRegularTabsFilter.prepareListOfTabsToBeClosed(timePeriod);
        if (mQuickDeleteArchivedTabsFilter != null) {
            mQuickDeleteArchivedTabsFilter.prepareListOfTabsToBeClosed(timePeriod);
        }

        mPropertyModel.set(
                QuickDeleteProperties.IS_SIGNED_IN, QuickDeleteDelegate.isSignedIn(mProfile));

        // Disable tabs if the user is in multi-window mode.
        // TODO(b/333036591): Remove this check once tab closure works properly across
        // multi-instances.
        if (!mPropertyModel.get(QuickDeleteProperties.HAS_MULTI_WINDOWS)) {
            mPropertyModel.set(
                    QuickDeleteProperties.CLOSED_TABS_COUNT, getCountOfTabsToBeDeleted());
        }

        mPropertyModel.set(QuickDeleteProperties.TIME_PERIOD, timePeriod);

        mPropertyModel.set(QuickDeleteProperties.IS_SYNCING_HISTORY, false);
        mPropertyModel.set(QuickDeleteProperties.IS_DOMAIN_VISITED_DATA_PENDING, true);
        // This is an async call which would update the browsing history row.
        mQuickDeleteBridge.getLastVisitedDomainAndUniqueDomainCount(timePeriod, this);
    }

    /**
     * Called when the domain count and last visited domain are fetched from local history.
     *
     * @param lastVisitedDomain The synced last visited domain on all devices within the selected
     *     time period.
     * @param domainCount The number of synced unique domains visited on all devices within the
     *     selected time period.
     */
    @Override
    public void onLastVisitedDomainAndUniqueDomainCountReady(
            String lastVisitedDomain, int domainCount) {
        mPropertyModel.set(QuickDeleteProperties.IS_DOMAIN_VISITED_DATA_PENDING, false);
        mPropertyModel.set(
                QuickDeleteProperties.IS_SYNCING_HISTORY,
                QuickDeleteDelegate.isSyncingHistory(mProfile));
        mPropertyModel.set(
                QuickDeleteProperties.DOMAIN_VISITED_DATA,
                new QuickDeleteDelegate.DomainVisitsData(lastVisitedDomain, domainCount));
    }

    private int getCountOfTabsToBeDeleted() {
        int count = mQuickDeleteRegularTabsFilter.getListOfTabsFilteredToBeClosed().size();
        if (mQuickDeleteArchivedTabsFilter != null) {
            count += mQuickDeleteArchivedTabsFilter.getListOfTabsFilteredToBeClosed().size();
        }
        return count;
    }
}
