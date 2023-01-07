// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.app.Activity;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/**
 * Factory for creating instances of the LightweightReactionsCoordinatorImpl.
 */
public class LightweightReactionsCoordinatorFactory {
    /**
     * @return a LightweightReactionsCoordinator instance.
     */
    public static LightweightReactionsCoordinator create(Activity activity,
            WindowAndroid windowAndroid, String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController) {
        Profile profile = Profile.getLastUsedRegularProfile();
        return new LightweightReactionsCoordinatorImpl(activity, windowAndroid, shareUrl,
                chromeOptionShareCallback, sheetController,
                ReactionServiceFactory.getForProfile(profile));
    }
}
