// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.graphics.Bitmap;
import android.graphics.Rect;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * Unit test of the {@link EntryManager} class. Note native implementation is mocked to make this a
 * unit test.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EntryManagerTest {
    private static final long FAKE_CAPTURE_ADDR = 123L;

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Tab mTabMock;
    @Mock private WebContents mWebContentsMock;
    @Mock private LongScreenshotsTabService mTabServiceMock;
    @Mock private ScreenshotBoundsManager mBoundsManagerMock;
    @Mock private LongScreenshotsCompositor mLongScreenshotsCompositorMock;
    @Mock private EntryManager.BitmapGeneratorObserver mObserverMock;
    @Mock private LongScreenshotsTabServiceFactory.Natives mLongScreenshotsTabServiceFactoryJniMock;
    @Mock private Bitmap mBitmapMock;

    private InOrder mInOrder;

    @Captor private ArgumentCaptor<Runnable> mErrorCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mCompleteCaptor;

    private EntryManager mEntryManager;
    private LongScreenshotsTabService.CaptureProcessor mProcessor;
    private BitmapGenerator mGenerator;
    private Callback<Integer> mOnCompositorResultCallback;

    @Before
    public void setUp() {
        initMocks(this);
        when(mTabMock.getWebContents()).thenReturn(mWebContentsMock);

        mJniMocker.mock(
                LongScreenshotsTabServiceFactoryJni.TEST_HOOKS,
                mLongScreenshotsTabServiceFactoryJniMock);
        mInOrder = inOrder(mTabServiceMock, mObserverMock);
        when(mLongScreenshotsTabServiceFactoryJniMock.getServiceInstanceForCurrentProfile())
                .thenReturn(mTabServiceMock);
        mEntryManager = new EntryManager(mBoundsManagerMock, mTabMock, false);
        mEntryManager.addBitmapGeneratorObserver(mObserverMock);
        final ArgumentCaptor<LongScreenshotsTabService.CaptureProcessor> captor =
                ArgumentCaptor.forClass(LongScreenshotsTabService.CaptureProcessor.class);
        mInOrder.verify(mTabServiceMock).setCaptureProcessor(captor.capture());
        mProcessor = captor.getValue();
        mInOrder.verify(mTabServiceMock).captureTab(eq(mTabMock), any(), eq(false));
        mInOrder.verify(mObserverMock).onStatusChange(eq(EntryStatus.CAPTURE_IN_PROGRESS));
        mGenerator = mEntryManager.getBitmapGeneratorForTesting();
        mGenerator.setCompositorFactoryForTesting(
                new BitmapGenerator.CompositorFactory() {
                    @Override
                    public LongScreenshotsCompositor create(
                            GURL url,
                            LongScreenshotsTabService tabService,
                            String directoryName,
                            long nativeCaptureResultPtr,
                            Callback<Integer> callback) {
                        assertNull(mOnCompositorResultCallback);
                        mOnCompositorResultCallback = callback;
                        return mLongScreenshotsCompositorMock;
                    }
                });
        when(mLongScreenshotsCompositorMock.requestBitmap(
                        any(), anyFloat(), mErrorCaptor.capture(), mCompleteCaptor.capture()))
                .thenReturn(0);
    }

    @After
    public void tearDown() {
        mEntryManager.removeBitmapGeneratorObserver(mObserverMock);
        mEntryManager.destroy();
    }

    /** Tests capture through to generation of the fullpage entry. */
    @Test
    public void testGenerateFullpageEntry() {
        mProcessor.processCapturedTab(FAKE_CAPTURE_ADDR, Status.OK);
        mOnCompositorResultCallback.onResult(CompositorStatus.OK);
        mInOrder.verify(mObserverMock).onStatusChange(eq(EntryStatus.CAPTURE_COMPLETE));
        mInOrder.verify(mObserverMock).onCompositorReady(any(), any());

        LongScreenshotsEntry entry = mEntryManager.generateFullpageEntry();
        assertEquals(EntryStatus.BITMAP_GENERATION_IN_PROGRESS, entry.getStatus());
        mCompleteCaptor.getValue().onResult(mBitmapMock);
        assertEquals(EntryStatus.BITMAP_GENERATED, entry.getStatus());
        assertEquals(entry.getBitmap(), mBitmapMock);
    }

    /** Tests capture through to generation of specified entry. */
    @Test
    public void testGenerateSpecificEntry() {
        mProcessor.processCapturedTab(FAKE_CAPTURE_ADDR, Status.OK);
        mOnCompositorResultCallback.onResult(CompositorStatus.OK);
        mInOrder.verify(mObserverMock).onStatusChange(eq(EntryStatus.CAPTURE_COMPLETE));
        mInOrder.verify(mObserverMock).onCompositorReady(any(), any());

        LongScreenshotsEntry entry = mEntryManager.generateEntry(new Rect(0, 0, 500, 500));
        assertEquals(EntryStatus.BITMAP_GENERATION_IN_PROGRESS, entry.getStatus());
        mCompleteCaptor.getValue().onResult(mBitmapMock);
        assertEquals(EntryStatus.BITMAP_GENERATED, entry.getStatus());
        assertEquals(entry.getBitmap(), mBitmapMock);
    }

    /** Tests capture through to generation of specified entry failing. */
    @Test
    public void testGenerateSpecificEntryFailed() {
        mProcessor.processCapturedTab(FAKE_CAPTURE_ADDR, Status.OK);
        mOnCompositorResultCallback.onResult(CompositorStatus.OK);
        mInOrder.verify(mObserverMock).onStatusChange(eq(EntryStatus.CAPTURE_COMPLETE));
        mInOrder.verify(mObserverMock).onCompositorReady(any(), any());

        LongScreenshotsEntry entry = mEntryManager.generateEntry(new Rect(0, 0, 500, 500));
        assertEquals(EntryStatus.BITMAP_GENERATION_IN_PROGRESS, entry.getStatus());
        mErrorCaptor.getValue().run();
        assertEquals(EntryStatus.GENERATION_ERROR, entry.getStatus());
        assertNull(entry.getBitmap());
    }

    /** Tests capture failure. */
    @Test
    public void testCaptureFailed() {
        mProcessor.processCapturedTab(FAKE_CAPTURE_ADDR, Status.CAPTURE_FAILED);
        mInOrder.verify(mTabServiceMock).releaseNativeCaptureResultPtr(eq(FAKE_CAPTURE_ADDR));
        mInOrder.verify(mObserverMock).onStatusChange(eq(EntryStatus.GENERATION_ERROR));
    }

    /** Tests capture failure due to low memory. */
    @Test
    public void testCaptureFailedLowMemory() {
        mProcessor.processCapturedTab(FAKE_CAPTURE_ADDR, Status.LOW_MEMORY_DETECTED);
        mInOrder.verify(mTabServiceMock).releaseNativeCaptureResultPtr(eq(FAKE_CAPTURE_ADDR));
        mInOrder.verify(mObserverMock).onStatusChange(eq(EntryStatus.INSUFFICIENT_MEMORY));
    }

    /** Tests compositor initialization failure. */
    @Test
    public void testCompositorFailed() {
        mProcessor.processCapturedTab(FAKE_CAPTURE_ADDR, Status.OK);
        mOnCompositorResultCallback.onResult(CompositorStatus.COMPOSITOR_SERVICE_DISCONNECT);
        mInOrder.verify(mObserverMock).onStatusChange(eq(EntryStatus.GENERATION_ERROR));
    }

    /** Tests compositor initialization failure due to low memory. */
    @Test
    public void testCompositorFailedLowMemory() {
        mProcessor.processCapturedTab(FAKE_CAPTURE_ADDR, Status.OK);
        mOnCompositorResultCallback.onResult(CompositorStatus.STOPPED_DUE_TO_MEMORY_PRESSURE);
        mInOrder.verify(mObserverMock).onStatusChange(eq(EntryStatus.INSUFFICIENT_MEMORY));
    }
}
