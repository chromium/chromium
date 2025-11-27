// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.MediaCapturePickerManager;
import org.chromium.chrome.browser.media.MediaCapturePickerTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
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

    @Mock private MediaCapturePickerTabObserver.Delegate mObserverDelegate;
    @Mock private MediaCapturePickerManager.Delegate mFilterDelegate;

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
        final var observer =
                new MediaCapturePickerTabObserver(mObserverDelegate, params, mFilterDelegate);

        final Tab tab = createMockTab(params.webContents, /* isNative= */ false);
        observer.onTabAdded(tab);
        verify(mObserverDelegate).onTabAdded(tab);
        verify(tab).addObserver(any());

        observer.onTabRemoved(tab);
        verify(mObserverDelegate).onTabRemoved(tab);
        verify(tab).removeObserver(any());
    }

    @Test
    @SmallTest
    public void testNativeTab() {
        final var params = new ParamsBuilder().build();
        final var observer =
                new MediaCapturePickerTabObserver(mObserverDelegate, params, mFilterDelegate);

        final Tab tab = createMockTab(/* webContents= */ null, /* isNative= */ true);
        observer.onTabAdded(tab);
        verify(mObserverDelegate, never()).onTabAdded(tab);
        verify(tab).addObserver(any());

        observer.onTabRemoved(tab);
        verify(mObserverDelegate, never()).onTabRemoved(tab);
        verify(tab).removeObserver(any());
    }

    @Test
    @SmallTest
    public void testCaptureThisTab() {
        final WebContents webContents = mock(WebContents.class);
        final var params =
                new ParamsBuilder().setWebContents(webContents).setCaptureThisTab(true).build();
        final var observer =
                new MediaCapturePickerTabObserver(mObserverDelegate, params, mFilterDelegate);

        final Tab thisTab = createMockTab(webContents, /* isNative= */ false);
        final Tab anotherTab = createMockTab(mock(WebContents.class), /* isNative= */ false);

        observer.onTabAdded(thisTab);
        verify(mObserverDelegate).onTabAdded(thisTab);
        verify(thisTab).addObserver(any());

        observer.onTabAdded(anotherTab);
        verify(mObserverDelegate, never()).onTabAdded(anotherTab);
        verify(anotherTab).addObserver(any());

        observer.onTabRemoved(thisTab);
        verify(mObserverDelegate).onTabRemoved(thisTab);
        verify(thisTab).removeObserver(any());

        observer.onTabRemoved(anotherTab);
        verify(mObserverDelegate, never()).onTabRemoved(anotherTab);
        verify(anotherTab).removeObserver(any());
    }

    @Test
    @SmallTest
    public void testPolicyFiltering() {
        final WebContents webContents = mock(WebContents.class);
        final var params = new ParamsBuilder().setWebContents(webContents).build();
        final var observer =
                new MediaCapturePickerTabObserver(mObserverDelegate, params, mFilterDelegate);

        // Tab that should be filtered.
        final WebContents filteredWebContents = mock(WebContents.class);
        final Tab filteredTab = createMockTab(filteredWebContents, /* isNative= */ false);
        when(mFilterDelegate.shouldFilterWebContents(filteredWebContents)).thenReturn(true);

        observer.onTabAdded(filteredTab);
        verify(mObserverDelegate, never()).onTabAdded(filteredTab);
        verify(filteredTab).addObserver(any());

        // Tab that should not be filtered.
        final WebContents allowedWebContents = mock(WebContents.class);
        final Tab allowedTab = createMockTab(allowedWebContents, /* isNative= */ false);
        when(mFilterDelegate.shouldFilterWebContents(allowedWebContents)).thenReturn(false);

        observer.onTabAdded(allowedTab);
        verify(mObserverDelegate).onTabAdded(allowedTab);
        verify(allowedTab).addObserver(any());
    }

    @Test
    @SmallTest
    public void testTabUpdate() {
        final var params = new ParamsBuilder().build();
        final var observer =
                new MediaCapturePickerTabObserver(mObserverDelegate, params, mFilterDelegate);

        final Tab tab = createMockTab(params.webContents, /* isNative= */ false);
        final ArgumentCaptor<TabObserver> tabObserverCaptor =
                ArgumentCaptor.forClass(TabObserver.class);

        observer.onTabAdded(tab);
        verify(mObserverDelegate).onTabAdded(tab);
        verify(tab).addObserver(tabObserverCaptor.capture());

        final TabObserver tabObserver = tabObserverCaptor.getValue();
        tabObserver.onTitleUpdated(tab);
        verify(mObserverDelegate).onTabUpdated(tab);
    }
}
