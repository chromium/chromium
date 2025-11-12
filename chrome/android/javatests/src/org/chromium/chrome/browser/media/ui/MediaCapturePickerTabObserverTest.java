// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

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
import org.chromium.chrome.browser.media.MediaCapturePickerManager;
import org.chromium.chrome.browser.media.MediaCapturePickerTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;

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

    private static class ParamsBuilder {
        private WebContents mWebContents = mock(WebContents.class);
        private boolean mCaptureThisTab;

        ParamsBuilder setWebContents(WebContents webContents) {
            mWebContents = webContents;
            return this;
        }

        ParamsBuilder setCaptureThisTab(boolean captureThisTab) {
            mCaptureThisTab = captureThisTab;
            return this;
        }

        MediaCapturePickerManager.Params build() {
            return new MediaCapturePickerManager.Params(
                    mWebContents,
                    "",
                    "",
                    /* requestAudio= */ false,
                    /* excludeSystemAudio= */ false,
                    /* windowAudioPreference= */ 0,
                    /* preferredDisplaySurface= */ 0,
                    mCaptureThisTab,
                    /* excludeSelfBrowserSurface= */ false,
                    /* excludeMonitorTypeSurfaces= */ false);
        }
    }

    private Tab createMockTab(WebContents webContents, boolean isNative) {
        final Tab tab = mock(Tab.class);
        when(tab.isNativePage()).thenReturn(isNative);
        when(tab.getWebContents()).thenReturn(webContents);
        return tab;
    }

    @Test
    @SmallTest
    public void testRegularTab() {
        final var params = new ParamsBuilder().build();
        final var observer = new MediaCapturePickerTabObserver(mDelegate, params);

        final Tab tab = createMockTab(params.webContents, /* isNative= */ false);
        observer.onTabAdded(tab);
        verify(mDelegate).onTabAdded(tab);

        observer.onTabRemoved(tab);
        verify(mDelegate).onTabRemoved(tab);
    }

    @Test
    @SmallTest
    public void testNativeTab() {
        final var params = new ParamsBuilder().build();
        final var observer = new MediaCapturePickerTabObserver(mDelegate, params);

        final Tab tab = createMockTab(/* webContents= */ null, /* isNative= */ true);
        observer.onTabAdded(tab);
        verify(mDelegate, never()).onTabAdded(tab);
        observer.onTabRemoved(tab);
        verify(mDelegate, never()).onTabRemoved(tab);
    }

    @Test
    @SmallTest
    public void testCaptureThisTab() {
        final WebContents webContents = mock(WebContents.class);
        final var params =
                new ParamsBuilder().setWebContents(webContents).setCaptureThisTab(true).build();
        final var observer = new MediaCapturePickerTabObserver(mDelegate, params);

        final Tab thisTab = createMockTab(webContents, /* isNative= */ false);
        final Tab anotherTab = createMockTab(mock(WebContents.class), /* isNative= */ false);

        observer.onTabAdded(thisTab);
        verify(mDelegate).onTabAdded(thisTab);

        observer.onTabAdded(anotherTab);
        verify(mDelegate, never()).onTabAdded(anotherTab);

        observer.onTabRemoved(thisTab);
        verify(mDelegate).onTabRemoved(thisTab);

        observer.onTabRemoved(anotherTab);
        verify(mDelegate, never()).onTabRemoved(anotherTab);
    }
}
