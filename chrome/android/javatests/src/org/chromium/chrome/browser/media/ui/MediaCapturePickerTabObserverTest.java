// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.MediaCapturePickerTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for the {@link MediaCapturePickerTabObserver}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS
})
@Batch(Batch.PER_CLASS)
public class MediaCapturePickerTabObserverTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AllTabObserver.Observer mDelegate;
    @Mock private Tab mNativeTab;
    @Mock private Tab mRegularTab;

    private MediaCapturePickerTabObserver mObserver;

    @Before
    public void setUp() {
        when(mNativeTab.isNativePage()).thenReturn(true);
        when(mRegularTab.isNativePage()).thenReturn(false);
        mObserver = new MediaCapturePickerTabObserver(mDelegate);
    }

    @Test
    @SmallTest
    public void testRegularTab() {
        mObserver.onTabAdded(mRegularTab);
        verify(mDelegate).onTabAdded(mRegularTab);

        mObserver.onTabRemoved(mRegularTab);
        verify(mDelegate).onTabRemoved(mRegularTab);
    }

    @Test
    @SmallTest
    public void testNativeTab() {
        mObserver.onTabAdded(mNativeTab);
        verify(mDelegate, never()).onTabAdded(mNativeTab);
        mObserver.onTabRemoved(mNativeTab);
        verify(mDelegate, never()).onTabRemoved(mNativeTab);
    }
}
