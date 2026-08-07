// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ReadAloudNativeBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ReadAloudNativeBridgeUnitTest {
    private static final long NATIVE_PTR = 12345L;
    private static final GURL TEST_GURL = JUnitTestGURLs.EXAMPLE_URL;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ReadAloudNativeBridge.Natives mNativeBridgeNatives;
    @Mock private Profile mProfile;
    @Mock private ReadAloudController mController;
    @Mock private WebContents mWebContents;

    private ReadAloudNativeBridge mBridge;

    @Before
    public void setUp() {
        ReadAloudNativeBridgeJni.setInstanceForTesting(mNativeBridgeNatives);
        when(mNativeBridgeNatives.init(any(), any())).thenReturn(NATIVE_PTR);
        when(mWebContents.isDestroyed()).thenReturn(false);

        mBridge = new ReadAloudNativeBridge();
    }

    @Test
    public void testInitializeAndIsInitialized() {
        assertFalse(mBridge.isInitialized());

        mBridge.initialize(mProfile, mController);

        assertTrue(mBridge.isInitialized());
        verify(mNativeBridgeNatives).init(eq(mProfile), eq(mBridge));
    }

    @Test
    public void testReinitialize_cleansUpPreviousInstance() {
        mBridge.initialize(mProfile, mController);
        assertTrue(mBridge.isInitialized());

        long secondNativePtr = 67890L;
        when(mNativeBridgeNatives.init(any(), any())).thenReturn(secondNativePtr);

        // Re-initializing should destroy the first native instance before creating the second.
        mBridge.initialize(mProfile, mController);

        verify(mNativeBridgeNatives).destroy(eq(NATIVE_PTR));
        verify(mNativeBridgeNatives, times(2)).init(eq(mProfile), eq(mBridge));
        assertTrue(mBridge.isInitialized());
    }

    @Test
    public void testDestroy() {
        mBridge.initialize(mProfile, mController);
        assertTrue(mBridge.isInitialized());

        mBridge.destroy();

        verify(mNativeBridgeNatives).destroy(eq(NATIVE_PTR));
        assertFalse(mBridge.isInitialized());

        // Subsequent destroy calls should be safe no-ops.
        mBridge.destroy();
        verify(mNativeBridgeNatives, times(1)).destroy(anyLong());
    }

    @Test
    public void testOutboundCommands_whenInitialized() {
        mBridge.initialize(mProfile, mController);

        mBridge.play(mWebContents);
        verify(mNativeBridgeNatives).play(eq(NATIVE_PTR), eq(mWebContents));

        mBridge.pause();
        verify(mNativeBridgeNatives).pause(eq(NATIVE_PTR));

        mBridge.stop();
        verify(mNativeBridgeNatives).stop(eq(NATIVE_PTR));

        mBridge.seekToWordIndex(42);
        verify(mNativeBridgeNatives).seekToWordIndex(eq(NATIVE_PTR), eq(42));

        mBridge.seek(5000L);
        verify(mNativeBridgeNatives).seek(eq(NATIVE_PTR), eq(5000L));

        mBridge.seekRelative(200L);
        verify(mNativeBridgeNatives).seekRelative(eq(NATIVE_PTR), eq(200L));

        mBridge.setPlaybackRate(1.5f);
        verify(mNativeBridgeNatives).setPlaybackRate(eq(NATIVE_PTR), eq(1.5f));

        mBridge.setVoice("voice_1");
        verify(mNativeBridgeNatives).setVoice(eq(NATIVE_PTR), eq("voice_1"));

        mBridge.previewVoice("voice_1");
        verify(mNativeBridgeNatives).previewVoice(eq(NATIVE_PTR), eq("voice_1"));

        mBridge.stopVoicePreview();
        verify(mNativeBridgeNatives).stopVoicePreview(eq(NATIVE_PTR));

        mBridge.setPlaybackMode(1);
        verify(mNativeBridgeNatives).setPlaybackMode(eq(NATIVE_PTR), eq(1));

        mBridge.setHighlightingEnabled(true);
        verify(mNativeBridgeNatives).setHighlightingEnabled(eq(NATIVE_PTR), eq(true));

        mBridge.sendFeedback(2);
        verify(mNativeBridgeNatives).sendFeedback(eq(NATIVE_PTR), eq(2));

        mBridge.checkReadability(TEST_GURL);
        verify(mNativeBridgeNatives).checkReadability(eq(NATIVE_PTR), eq(TEST_GURL));
    }

    @Test
    public void testOutboundCommands_whenUninitialized_safeNoOps() {
        assertFalse(mBridge.isInitialized());

        mBridge.play(mWebContents);
        mBridge.pause();
        mBridge.stop();
        mBridge.seekToWordIndex(42);
        mBridge.seek(5000L);
        mBridge.seekRelative(200L);
        mBridge.setPlaybackRate(1.5f);
        mBridge.setVoice("voice_1");
        mBridge.previewVoice("voice_1");
        mBridge.stopVoicePreview();
        mBridge.setPlaybackMode(1);
        mBridge.setHighlightingEnabled(true);
        mBridge.sendFeedback(2);
        mBridge.checkReadability(TEST_GURL);

        verify(mNativeBridgeNatives, never()).play(anyLong(), any());
        verify(mNativeBridgeNatives, never()).pause(anyLong());
        verify(mNativeBridgeNatives, never()).stop(anyLong());
        verify(mNativeBridgeNatives, never()).seekToWordIndex(anyLong(), anyInt());
        verify(mNativeBridgeNatives, never()).seek(anyLong(), anyLong());
        verify(mNativeBridgeNatives, never()).seekRelative(anyLong(), anyLong());
        verify(mNativeBridgeNatives, never()).setPlaybackRate(anyLong(), anyFloat());
        verify(mNativeBridgeNatives, never()).setVoice(anyLong(), anyString());
        verify(mNativeBridgeNatives, never()).previewVoice(anyLong(), anyString());
        verify(mNativeBridgeNatives, never()).stopVoicePreview(anyLong());
        verify(mNativeBridgeNatives, never()).setPlaybackMode(anyLong(), anyInt());
        verify(mNativeBridgeNatives, never()).setHighlightingEnabled(anyLong(), anyBoolean());
        verify(mNativeBridgeNatives, never()).sendFeedback(anyLong(), anyInt());
        verify(mNativeBridgeNatives, never()).checkReadability(anyLong(), any());
    }

    @Test
    public void testPlay_destroyedOrNullWebContents_preventDispatch() {
        mBridge.initialize(mProfile, mController);

        mBridge.play(null);
        verify(mNativeBridgeNatives, never()).play(anyLong(), any());

        when(mWebContents.isDestroyed()).thenReturn(true);
        mBridge.play(mWebContents);
        verify(mNativeBridgeNatives, never()).play(anyLong(), any());
    }

    @Test
    public void testCheckReadability_nullUrl_preventDispatch() {
        mBridge.initialize(mProfile, mController);

        mBridge.checkReadability(null);
        verify(mNativeBridgeNatives, never()).checkReadability(anyLong(), any());
    }

    @Test
    public void testInboundCallbacks_forwardToController() {
        mBridge.initialize(mProfile, mController);

        mBridge.onMetadataAvailable("Article Title", "Publisher Name");
        verify(mController).onMetadataAvailable("Article Title", "Publisher Name");

        mBridge.onPlaybackProgressUpdated(1000L, 5000L);
        verify(mController).onPlaybackProgressUpdated(1000L, 5000L);

        mBridge.onPlaybackStateChanged(2);
        verify(mController).onPlaybackStateChanged(2);

        String[] ids = new String[] {"id1", "id2"};
        String[] names = new String[] {"Name 1", "Name 2"};
        mBridge.onVoicesAvailable(ids, names, "id1");
        verify(mController).onVoicesAvailable(ids, names, "id1");

        mBridge.onWordHighlightUpdated(10, 20);
        verify(mController).onWordHighlightUpdated(10, 20);

        mBridge.onHighlightingSupported(true);
        verify(mController).onHighlightingSupported(true);

        mBridge.onFallbackEngaged();
        verify(mController).onFallbackEngaged();

        mBridge.onPlaybackError("Synthesis error");
        verify(mController).onPlaybackError("Synthesis error");

        mBridge.onVoicePreviewPlaybackStateChanged("id1", 1);
        verify(mController).onVoicePreviewPlaybackStateChanged("id1", 1);

        mBridge.onReadabilityResult(TEST_GURL, true);
        verify(mController).onReadabilityResult(TEST_GURL, true);

        mBridge.onNativeDestroyed();
        verify(mController).onNativeDestroyed();
        assertFalse(mBridge.isInitialized());
    }

    @Test
    public void testInboundCallbacks_whenControllerIsNull_safeNoOps() {
        // Calling inbound callbacks without initializing controller must not crash.
        mBridge.onMetadataAvailable("Title", "Publisher");
        mBridge.onPlaybackProgressUpdated(100L, 500L);
        mBridge.onPlaybackStateChanged(1);
        mBridge.onVoicesAvailable(new String[0], new String[0], "");
        mBridge.onWordHighlightUpdated(0, 5);
        mBridge.onHighlightingSupported(false);
        mBridge.onFallbackEngaged();
        mBridge.onPlaybackError("Error");
        mBridge.onVoicePreviewPlaybackStateChanged("voice", 0);
        mBridge.onReadabilityResult(TEST_GURL, false);
        mBridge.onNativeDestroyed();

        verifyNoInteractions(mController);
    }
}
