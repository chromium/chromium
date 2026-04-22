// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.componentinterfaces.SurfaceCoordinator.StreamTabId;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;

/** Unit tests for {@link FeedFeatures}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FeedFeaturesTest {
    @Rule public MockitoRule rule = MockitoJUnit.rule().strictness(Strictness.WARN);

    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;

    private @StreamTabId int mPrefStoredTab;

    @Before
    public void setUp() {
        // Defaults the stored pref to return the Following feed.
        mPrefStoredTab = StreamTabId.FOLLOWING;
        // mPrefService = mock(PrefService.class);
        FeedFeatures.setFakePrefsForTest(mPrefService);
        when(mPrefService.getInteger(Pref.LAST_SEEN_FEED_TYPE))
                .thenAnswer((InvocationOnMock i) -> mPrefStoredTab);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            Object[] args = invocation.getArguments();
                            mPrefStoredTab = (Integer) args[1];
                            return null;
                        })
                .when(mPrefService)
                .setInteger(eq(Pref.LAST_SEEN_FEED_TYPE), anyInt());
    }

    @Test
    public void testAlwaysResetByDefault() {
        assertEquals(StreamTabId.FOR_YOU, FeedFeatures.getFeedTabIdToRestore(mProfile));
        assertEquals(StreamTabId.FOR_YOU, mPrefStoredTab);
        // Simulates a Following tab selection.
        FeedFeatures.setLastSeenFeedTabId(mProfile, StreamTabId.FOLLOWING);
        assertEquals(StreamTabId.FOR_YOU, FeedFeatures.getFeedTabIdToRestore(mProfile));
        assertEquals(StreamTabId.FOR_YOU, mPrefStoredTab);
    }
}
