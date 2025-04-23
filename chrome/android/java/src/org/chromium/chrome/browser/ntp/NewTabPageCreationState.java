// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UserData;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/** UserData that tracks the creation state of a new tab page. */
public class NewTabPageCreationState implements UserData {
    private static NewTabPageCreationState sInstanceForTesting;

    private boolean mIsNewlyCreated;
    private boolean mNtpLoaded;

    private @Nullable NewTabPageManager mNewTabPageManager;

    /**
     * Returns the {@link NewTabPageCreationState} for the given {@link Tab}, creating one if
     * necessary.
     *
     * @param tab The {@link Tab} to get the state for.
     */
    static @Nullable NewTabPageCreationState from(Tab tab) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_OMNIBOX_FOCUSED_NEW_TAB_PAGE)) {
            return null;
        }

        // TODO(crbug.com/408364837): Track NTP module usage and only create this for targeted user
        // group.

        if (sInstanceForTesting != null) return sInstanceForTesting;

        NewTabPageCreationState state =
                tab.getUserDataHost().getUserData(NewTabPageCreationState.class);
        if (state == null) {
            state = new NewTabPageCreationState();
            tab.getUserDataHost().setUserData(NewTabPageCreationState.class, state);
        }
        return state;
    }

    /** Update the state when new NTPs opened in a new tab. */
    void onNewTabCreated() {
        mIsNewlyCreated = true;
        maybeFocusOmnibox();
    }

    /** Update the state when NTP finish loading. */
    void onNtpLoaded(NewTabPageManager newTabPageManager) {
        mNtpLoaded = true;
        mNewTabPageManager = newTabPageManager;
        maybeFocusOmnibox();
    }

    private void maybeFocusOmnibox() {
        if (mIsNewlyCreated && mNtpLoaded) {
            mIsNewlyCreated = false;
            mNewTabPageManager.focusSearchBox(false, null);
        }
    }

    static void setInstanceForTesting(NewTabPageCreationState instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
