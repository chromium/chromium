// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator responsible for listening to back-end changes affecting the quick delete {@link
 * View}.
 */
class QuickDeleteMediator implements QuickDeleteDialogDelegate.TimePeriodChangeObserver,
                                     QuickDeleteBridge.DomainVisitsCallback {
    private final @NonNull PropertyModel mPropertyModel;
    private final @NonNull Profile mProfile;
    private final @NonNull QuickDeleteBridge mQuickDeleteBridge;
    private final @NonNull QuickDeleteTabsFilter mQuickDeleteTabsFilter;

    /**
     * @param propertyModel {@link PropertyModel} associated with the quick delete {@link View}.
     * @param profile {@link Profile} to check if the user is signed-in or syncing.
     * @param quickDeleteBridge {@link QuickDeleteBridge} used to fetch the recent visited domain
     *         and site data.
     * @param quickDeleteTabsFilter {@link QuickDeleteTabsFilter} used to fetch the tabs to be
     *         closed data.
     */
    QuickDeleteMediator(@NonNull PropertyModel propertyModel, @NonNull Profile profile,
            @NonNull QuickDeleteBridge quickDeleteBridge,
            @NonNull QuickDeleteTabsFilter quickDeleteTabsFilter) {
        mPropertyModel = propertyModel;
        mProfile = profile;
        mQuickDeleteBridge = quickDeleteBridge;
        mQuickDeleteTabsFilter = quickDeleteTabsFilter;
    }

    /**
     * A callback which is fired when the user updates the time period spinner inside quick delete.
     *
     * @param timePeriod The latest {@link TimePeriod} in the toggle.
     */
    @Override
    public void onTimePeriodChanged(@TimePeriod int timePeriod) {
        mPropertyModel.set(
                QuickDeleteProperties.IS_SIGNED_IN, QuickDeleteDelegate.isSignedIn(mProfile));
        mPropertyModel.set(QuickDeleteProperties.CLOSED_TABS_COUNT,
                mQuickDeleteTabsFilter.getListOfTabsToBeClosed(timePeriod).size());
        mPropertyModel.set(QuickDeleteProperties.TIME_PERIOD, timePeriod);

        mPropertyModel.set(QuickDeleteProperties.IS_SYNCING_HISTORY, false);
        mPropertyModel.set(QuickDeleteProperties.IS_DOMAIN_VISITED_DATA_PENDING, true);
        // This is an async call which would update the browsing history row.
        mQuickDeleteBridge.getLastVisitedDomainAndUniqueDomainCount(timePeriod, this);
    }

    /**
     * Called when the domain count and last visited domain are fetched from local history.
     *
     * @param lastVisitedDomain The synced last visited domain on all devices in the last 15
     *                          minutes.
     * @param domainCount The number of synced unique domains visited on all devices in the
     *                    last 15 minutes.
     */
    @Override
    public void onLastVisitedDomainAndUniqueDomainCountReady(
            String lastVisitedDomain, int domainCount) {
        mPropertyModel.set(QuickDeleteProperties.IS_DOMAIN_VISITED_DATA_PENDING, false);
        mPropertyModel.set(QuickDeleteProperties.IS_SYNCING_HISTORY,
                QuickDeleteDelegate.isSyncingHistory(mProfile));
        mPropertyModel.set(QuickDeleteProperties.DOMAIN_VISITED_DATA,
                new QuickDeleteDelegate.DomainVisitsData(lastVisitedDomain, domainCount));
    }
}
