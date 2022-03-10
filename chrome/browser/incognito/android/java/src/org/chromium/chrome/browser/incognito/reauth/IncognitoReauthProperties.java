// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

class IncognitoReauthProperties {
    public static final ReadableObjectPropertyKey<Runnable> ON_UNLOCK_INCOGNITO_CLICKED =
            new ReadableObjectPropertyKey<>("on_unlock_incognito_clicked");
    public static final ReadableObjectPropertyKey<Runnable> ON_SEE_OTHER_TABS_CLICKED =
            new ReadableObjectPropertyKey<>("on_see_other_tabs_clicked");
    public static final ReadableBooleanPropertyKey IS_FULL_SCREEN =
            new ReadableBooleanPropertyKey();
    public static final PropertyKey[] ALL_KEYS = {
            ON_UNLOCK_INCOGNITO_CLICKED, ON_SEE_OTHER_TABS_CLICKED, IS_FULL_SCREEN};

    /**
     * Creates an instance of {@link PropertyModel} for the incognito re-auth view.
     *
     * @param unlockIncognitoRunnable The runnable that would be run when the user clicks on the
     *         "Unlock Incognito" button.
     * @param seeOtherTabsRunnable The runnable that would be run when the user clicks on the "See
     *         other tabs" button.
     * @param fullscreen A boolean indicating whether the incognito re-auth view needs to be shown
     *         fullscreen style or tab-switcher style.
     *
     * @return A {@link PropertyModel} instance for the incognito re-auth view with the above
     *         attributes.
     */
    public static PropertyModel createPropertyModel(
            Runnable unlockIncognitoRunnable, Runnable seeOtherTabsRunnable, boolean fullscreen) {
        return new PropertyModel.Builder(IncognitoReauthProperties.ALL_KEYS)
                .with(IncognitoReauthProperties.ON_UNLOCK_INCOGNITO_CLICKED,
                        unlockIncognitoRunnable)
                .with(IncognitoReauthProperties.ON_SEE_OTHER_TABS_CLICKED, seeOtherTabsRunnable)
                .with(IncognitoReauthProperties.IS_FULL_SCREEN, fullscreen)
                .build();
    }
}
