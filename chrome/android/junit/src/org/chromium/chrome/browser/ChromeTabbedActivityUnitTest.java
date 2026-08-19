// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;

/** Unit tests for {@link ChromeTabbedActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeTabbedActivityUnitTest {
    private ChromeTabbedActivity mActivity;

    @Before
    public void setUp() {
        mActivity = new ChromeTabbedActivity();
        ProfileManager.resetForTesting();
    }

    @After
    public void tearDown() {
        ProfileManager.resetForTesting();
    }

    @Test
    public void testTransformSavedInstanceStateForOnCreate_nullState() {
        assertNull(mActivity.transformSavedInstanceStateForOnCreate(null));
    }

    @Test
    public void testTransformSavedInstanceStateForOnCreate_profileManagerNotInitialized() {
        assertFalse(ProfileManager.isInitialized());

        Bundle savedState = new Bundle();
        savedState.putBundle("android:support:fragments", new Bundle());
        savedState.putString("custom_key", "custom_value");

        assertNull(mActivity.transformSavedInstanceStateForOnCreate(savedState));
    }

    @Test
    public void testTransformSavedInstanceStateForOnCreate_profileManagerInitialized() {
        Profile profile = mock(Profile.class);
        ProfileManager.setLastUsedProfileForTesting(profile);
        assertTrue(ProfileManager.isInitialized());

        Bundle savedState = new Bundle();
        savedState.putBundle("android:support:fragments", new Bundle());
        savedState.putString("custom_key", "custom_value");

        Bundle result = mActivity.transformSavedInstanceStateForOnCreate(savedState);

        assertNotNull(result);
        assertTrue(result.containsKey("android:support:fragments"));
        assertEquals("custom_value", result.getString("custom_key"));
    }
}
