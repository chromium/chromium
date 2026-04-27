// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshotprovider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

import java.util.function.Supplier;

/** Unit tests for {@link ScreenshotInvocationState}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ScreenshotInvocationStateUnitTest {

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private RenderCoordinatesImpl mRenderCoordinates;
    private Supplier<@Nullable Tab> mTabSupplier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabSupplier = () -> mTab;
        RenderCoordinatesImpl.setInstanceForTesting(mRenderCoordinates);

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(JUnitTestGURLs.URL_1).when(mTab).getUrl();
        when(mRenderCoordinates.getScrollXPixInt()).thenReturn(100);
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(200);
    }

    @Test
    public void testCreateSuccess() {
        ScreenshotInvocationState state = ScreenshotInvocationState.create(mTabSupplier);
        assertNotNull(state);
        assertEquals(JUnitTestGURLs.URL_1.getSpec(), state.getInvokedUrl());
    }

    @Test
    public void testCreateFailure_NullTab() {
        assertNull(ScreenshotInvocationState.create(() -> null));
    }

    @Test
    public void testInitState() {
        ScreenshotInvocationState state = ScreenshotInvocationState.create(mTabSupplier);
        state.initState();
        assertNotNull(state.getInvocationId());
        assertNotNull(state.getContentUri());
        assertTrue(state.getContentUri().toString().contains(state.getInvocationId()));
    }

    @Test
    public void testCanReuse_SameState() {
        ScreenshotInvocationState state = ScreenshotInvocationState.create(mTabSupplier);
        assertTrue(state.canReuse(state));
    }

    @Test
    public void testCanReuse_UrlMismatch() {
        ScreenshotInvocationState state1 = ScreenshotInvocationState.create(mTabSupplier);

        doReturn(JUnitTestGURLs.URL_2).when(mTab).getUrl();
        ScreenshotInvocationState state2 = ScreenshotInvocationState.create(mTabSupplier);

        assertFalse(state2.canReuse(state1));
    }

    @Test
    public void testCanReuse_ScrollMismatch() {
        ScreenshotInvocationState state1 = ScreenshotInvocationState.create(mTabSupplier);

        // Change scroll significantly (threshold is 20)
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(250);
        ScreenshotInvocationState state2 = ScreenshotInvocationState.create(mTabSupplier);

        assertFalse(state2.canReuse(state1));
    }

    @Test
    public void testCanReuse_ScrollWithinThreshold() {
        ScreenshotInvocationState state1 = ScreenshotInvocationState.create(mTabSupplier);

        // Change scroll slightly
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(210);
        ScreenshotInvocationState state2 = ScreenshotInvocationState.create(mTabSupplier);

        assertTrue(state2.canReuse(state1));
    }

    @Test
    public void testCanReuse_Expired() {
        ScreenshotInvocationState state1 = ScreenshotInvocationState.create(mTabSupplier);

        // Advance time beyond threshold (REUSE_URI_MAX_AGE_MS = 60 seconds)
        org.robolectric.shadows.ShadowSystemClock.advanceBy(java.time.Duration.ofSeconds(61));

        ScreenshotInvocationState state2 = ScreenshotInvocationState.create(mTabSupplier);
        assertFalse(state2.canReuse(state1));
    }
}
