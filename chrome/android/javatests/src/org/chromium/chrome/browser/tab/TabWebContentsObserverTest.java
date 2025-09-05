// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;

import static org.chromium.build.NullUtil.assertNonNull;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tab.Tab.MediaState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.content_public.browser.WebContentsObserver;

/** Tests for {@link TabWebContentsObserver}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabWebContentsObserverTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    private Tab mTab;
    private TabWebContentsObserver mTabWebContentsObserver;
    private WebContentsObserver mObserver;

    @Before
    public void setUp() {
        mActivityTestRule.startOnNtp();
        mTab = mActivityTestRule.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabWebContentsObserver = TabWebContentsObserver.from(mTab);
                });
        mObserver = mTabWebContentsObserver.getWebContentsObserverForTesting();
        assertNonNull(mObserver);
    }

    @Test
    @SmallTest
    public void testMediaStartedPlaying() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(MediaState.NONE, mTab.getMediaState());
                    mObserver.mediaStartedPlaying();
                    assertEquals(MediaState.AUDIBLE, mTab.getMediaState());
                });
    }

    @Test
    @SmallTest
    public void testMediaStoppedPlaying() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mObserver.mediaStartedPlaying();
                    assertEquals(MediaState.AUDIBLE, mTab.getMediaState());
                    mObserver.mediaStoppedPlaying();
                    assertEquals(MediaState.NONE, mTab.getMediaState());
                });
    }

    @Test
    @SmallTest
    public void testDidUpdateAudioMutingState() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(MediaState.NONE, mTab.getMediaState());
                    mObserver.didUpdateAudioMutingState(true);
                    assertEquals(MediaState.NONE, mTab.getMediaState());

                    mTab.setMediaState(MediaState.AUDIBLE);
                    mObserver.didUpdateAudioMutingState(true);
                    assertEquals(MediaState.MUTED, mTab.getMediaState());

                    mObserver.didUpdateAudioMutingState(false);
                    assertEquals(MediaState.AUDIBLE, mTab.getMediaState());

                    mTab.setMediaState(MediaState.NONE);
                    mObserver.didUpdateAudioMutingState(false);
                    assertEquals(MediaState.NONE, mTab.getMediaState());
                });
    }
}
