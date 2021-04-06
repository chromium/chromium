// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.chrome.browser.incognito.IncognitoUtils.getNonPrimaryOTRProfileFromWindowAndroid;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelImpl.IncognitoTabModelDelegate;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * Stores all the variables needed to create an Incognito TabModelImpl when it is needed.
 */
class IncognitoTabModelImplCreator implements IncognitoTabModelDelegate {
    private final TabCreator mRegularTabCreator;
    private final TabCreator mIncognitoTabCreator;
    private final TabModelOrderController mOrderController;
    private final TabContentManager mTabContentManager;
    private final NextTabPolicySupplier mNextTabPolicySupplier;
    private final AsyncTabParamsManager mAsyncTabParamsManager;
    private final TabModelDelegate mModelDelegate;

    // This is passed in as null if the {@link WindowAndroid} instance doesn't belong to an
    // incognito CustomTabActivity.
    @Nullable
    private final Supplier<WindowAndroid> mWindowAndroidSupplier;

    private final @ActivityType int mActivityType;
    /**
     * Constructor for an IncognitoTabModelImplCreator, used by {@link IncognitoTabModelImpl}.
     *
     * Creating an instance of this class does not create the Incognito TabModelImpl immediately.
     * The {@link IncognitoTabModelImpl} will use this class to create the real TabModelImpl when it
     * will actually be used.
     *
     * @param windowAndroidSupplier The supplier to the {@link WindowAndroid} instance.
     * @param regularTabCreator   Creates regular tabs.
     * @param incognitoTabCreator Creates incognito tabs.
     * @param orderController     Determines the order for inserting new Tabs.
     * @param tabContentManager   Manages the display content of the tab.
     * @param nextTabPolicySupplier Supplies the policy to pick a next tab if the current is closed
     * @param asyncTabParamsManager An {@link AsyncTabParamsManager} instance.
     * @param activityType Type of the activity for the tab model.
     * @param modelDelegate Delegate to handle external dependencies and interactions.
     */
    IncognitoTabModelImplCreator(@Nullable Supplier<WindowAndroid> windowAndroidSupplier,
            TabCreator regularTabCreator, TabCreator incognitoTabCreator,
            TabModelOrderController orderController, TabContentManager tabContentManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager, @ActivityType int activityType,
            TabModelDelegate modelDelegate) {
        mWindowAndroidSupplier = windowAndroidSupplier;
        mRegularTabCreator = regularTabCreator;
        mIncognitoTabCreator = incognitoTabCreator;
        mOrderController = orderController;
        mTabContentManager = tabContentManager;
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mActivityType = activityType;
        mModelDelegate = modelDelegate;
    }

    private @NonNull Profile getOTRProfile() {
        if (mWindowAndroidSupplier != null) {
            Profile otrProfile =
                    getNonPrimaryOTRProfileFromWindowAndroid(mWindowAndroidSupplier.get());

            // TODO(crbug.com/1023759): PaymentHandlerActivity is an exceptional case that uses the
            // primary OTR profile. PaymentHandlerActivity would use incognito CCT when the
            // Incognito CCT flag is enabled by default in which case we would return the non
            // primary OTR profile.
            if (otrProfile == null) {
                return Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(
                        /*createIfNeeded=*/true);
            }

            return otrProfile;
        }
        return Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(/*createIfNeeded=*/true);
    }

    @Override
    public TabModel createTabModel() {
        return new TabModelImpl(getOTRProfile(), mActivityType, mRegularTabCreator,
                mIncognitoTabCreator, mOrderController, mTabContentManager, mNextTabPolicySupplier,
                mAsyncTabParamsManager, mModelDelegate, false);
    }

}
