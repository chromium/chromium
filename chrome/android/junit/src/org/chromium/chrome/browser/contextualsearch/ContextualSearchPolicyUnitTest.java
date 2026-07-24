// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Tests for {@link ContextualSearchPolicy} verifying desktop disable logic. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextualSearchPolicyUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;

    @Before
    public void setUp() {
        UserPrefs.setPrefServiceForTesting(mPrefService);
    }

    @Test
    public void testIsContextualSearchEnabled_Normal_PrefEnabled() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        when(mPrefService.getString(Pref.CONTEXTUAL_SEARCH_ENABLED)).thenReturn("true");

        assertTrue(ContextualSearchPolicy.isContextualSearchEnabled(mProfile));
        assertFalse(ContextualSearchPolicy.isContextualSearchDisabled(mProfile));
        assertFalse(ContextualSearchPolicy.isContextualSearchUninitialized(mProfile));
    }

    @Test
    public void testIsContextualSearchEnabled_Normal_PrefDisabled() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        when(mPrefService.getString(Pref.CONTEXTUAL_SEARCH_ENABLED)).thenReturn("false");

        assertFalse(ContextualSearchPolicy.isContextualSearchEnabled(mProfile));
        assertTrue(ContextualSearchPolicy.isContextualSearchDisabled(mProfile));
        assertFalse(ContextualSearchPolicy.isContextualSearchUninitialized(mProfile));
    }

    @Test
    public void testIsContextualSearchEnabled_Normal_PrefUninitialized() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(false);
        when(mPrefService.getString(Pref.CONTEXTUAL_SEARCH_ENABLED)).thenReturn("");

        assertFalse(ContextualSearchPolicy.isContextualSearchEnabled(mProfile));
        assertFalse(ContextualSearchPolicy.isContextualSearchDisabled(mProfile));
        assertTrue(ContextualSearchPolicy.isContextualSearchUninitialized(mProfile));
    }

    @Test
    public void testIsContextualSearchEnabled_Desktop_PrefEnabled() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        when(mPrefService.getString(Pref.CONTEXTUAL_SEARCH_ENABLED)).thenReturn("true");

        assertFalse(ContextualSearchPolicy.isContextualSearchEnabled(mProfile));
        assertTrue(ContextualSearchPolicy.isContextualSearchDisabled(mProfile));
        assertFalse(ContextualSearchPolicy.isContextualSearchUninitialized(mProfile));
    }

    @Test
    public void testIsContextualSearchEnabled_Desktop_PrefDisabled() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        when(mPrefService.getString(Pref.CONTEXTUAL_SEARCH_ENABLED)).thenReturn("false");

        assertFalse(ContextualSearchPolicy.isContextualSearchEnabled(mProfile));
        assertTrue(ContextualSearchPolicy.isContextualSearchDisabled(mProfile));
        assertFalse(ContextualSearchPolicy.isContextualSearchUninitialized(mProfile));
    }

    @Test
    public void testIsContextualSearchEnabled_Desktop_PrefUninitialized() {
        OmniboxCapabilities.setIsDesktopPlatformForTesting(true);
        when(mPrefService.getString(Pref.CONTEXTUAL_SEARCH_ENABLED)).thenReturn("");

        assertFalse(ContextualSearchPolicy.isContextualSearchEnabled(mProfile));
        assertTrue(ContextualSearchPolicy.isContextualSearchDisabled(mProfile));
        assertFalse(ContextualSearchPolicy.isContextualSearchUninitialized(mProfile));
    }
}
