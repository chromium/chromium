// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SearchProviderInfoDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchProviderInfoDelegateUnitTest {
    private SearchProviderInfoDelegate mDelegate;

    @Before
    public void setUp() {
        mDelegate = new SearchProviderInfoDelegate();
    }

    @Test
    public void testDefaultValues() {
        assertTrue(mDelegate.getSearchProviderHasLogo());
        assertFalse(mDelegate.getSearchProviderIsGoogle());
        assertFalse(mDelegate.getShowingNonStandardGoogleLogo());
    }

    @Test
    public void testSetSearchProviderInfo() {
        mDelegate.setShowingNonStandardGoogleLogo(/* showingNonStandardGoogleLogo= */ true);
        assertTrue(mDelegate.setSearchProviderInfo(/* hasLogo= */ false, /* isGoogle= */ true));
        assertFalse(mDelegate.getSearchProviderHasLogo());
        assertTrue(mDelegate.getSearchProviderIsGoogle());
        assertTrue(mDelegate.getShowingNonStandardGoogleLogo());

        // Calling with the same parameters returns false.
        assertFalse(mDelegate.setSearchProviderInfo(/* hasLogo= */ false, /* isGoogle= */ true));

        // Setting isGoogle to false resets mShowingNonStandardGoogleLogo to false.
        assertTrue(mDelegate.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ false));
        assertTrue(mDelegate.getSearchProviderHasLogo());
        assertFalse(mDelegate.getSearchProviderIsGoogle());
        assertFalse(mDelegate.getShowingNonStandardGoogleLogo());
    }

    @Test
    public void testSettersAndGetters() {
        mDelegate.setSearchProviderHasLogo(/* hasLogo= */ false);
        assertFalse(mDelegate.getSearchProviderHasLogo());

        mDelegate.setSearchProviderHasLogo(/* hasLogo= */ true);
        assertTrue(mDelegate.getSearchProviderHasLogo());

        mDelegate.setSearchProviderIsGoogle(/* isGoogle= */ true);
        assertTrue(mDelegate.getSearchProviderIsGoogle());

        mDelegate.setSearchProviderIsGoogle(/* isGoogle= */ false);
        assertFalse(mDelegate.getSearchProviderIsGoogle());

        mDelegate.setShowingNonStandardGoogleLogo(/* showingNonStandardGoogleLogo= */ true);
        assertTrue(mDelegate.getShowingNonStandardGoogleLogo());

        mDelegate.setShowingNonStandardGoogleLogo(/* showingNonStandardGoogleLogo= */ false);
        assertFalse(mDelegate.getShowingNonStandardGoogleLogo());
    }
}
