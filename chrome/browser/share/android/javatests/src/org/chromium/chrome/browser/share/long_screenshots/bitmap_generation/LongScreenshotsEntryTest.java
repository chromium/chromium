// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;

/** Tests for the LongScreenshotsEntry. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT)
public class LongScreenshotsEntryTest {
    @Mock
    private Context mContext;

    @Mock
    private Tab mTab;

    @Mock
    private RenderCoordinatesImpl mRenderCoordinates;

    @Mock
    private WebContentsImpl mWebContents;

    @Mock
    private LongScreenshotsCompositor mCompositor;

    @Mock
    private LongScreenshotsTabService mTabService;

    private Bitmap mTestBitmap = Bitmap.createBitmap(512, 1024, Bitmap.Config.ARGB_8888);

    class TestEntryListener implements LongScreenshotsEntry.EntryListener {
        @EntryStatus
        int mReturnedStatus;

        @Override
        public void onResult(@EntryStatus int status) {
            mReturnedStatus = status;
        }

        public @EntryStatus int getReturnedStatus() {
            return mReturnedStatus;
        }
    }

    class TestBitmapGenerator extends BitmapGenerator {
        private boolean mThrowErrorOnComposite;

        public TestBitmapGenerator(Rect rect) {
            super(mContext, mTab, rect, null);
            setTabServiceAndCompositorForTest(mTabService, mCompositor);
        }

        void throwErrorOnComposite() {
            mThrowErrorOnComposite = true;
        }

        public void setCompositorStatus(@CompositorStatus int status) {
            mGeneratorCallBack.onCompositorResult(status);
        }

        public void setCaptureStatus(@Status int status) {
            mGeneratorCallBack.onCaptureResult(status);
        }

        @Override
        public int compositeBitmap(
                Rect rect, Runnable errorCallback, Callback<Bitmap> onBitmapGenerated) {
            if (mThrowErrorOnComposite) {
                errorCallback.run();
                return -1;
            }
            onBitmapGenerated.onResult(mTestBitmap);
            return 1;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mRenderCoordinates.getPageScaleFactorInt()).thenReturn(1);
        when(mRenderCoordinates.getScrollYPixInt()).thenReturn(100);
        when(mRenderCoordinates.getContentWidthPixInt()).thenReturn(200);
    }

    @Test
    public void testSuccessfulEntry() {
        TestBitmapGenerator testGenerator = new TestBitmapGenerator(new Rect(0, 0, 200, 1000));

        LongScreenshotsEntry entry =
                new LongScreenshotsEntry(mContext, mTab, 0, 1000, testGenerator, false);
        TestEntryListener entryListener = new TestEntryListener();
        entry.setListener(entryListener);
        entry.generateBitmap();

        assertEquals(mTestBitmap, entry.getBitmap());
        assertEquals(EntryStatus.BITMAP_GENERATED, entryListener.getReturnedStatus());
    }

    @Test
    public void testBitmapGenerationError() {
        TestBitmapGenerator testGenerator = new TestBitmapGenerator(new Rect(0, 0, 200, 1000));
        testGenerator.throwErrorOnComposite();

        LongScreenshotsEntry entry =
                new LongScreenshotsEntry(mContext, mTab, 0, 1000, testGenerator, false);
        TestEntryListener entryListener = new TestEntryListener();
        entry.setListener(entryListener);
        entry.generateBitmap();

        assertNull(entry.getBitmap());
        assertEquals(EntryStatus.GENERATION_ERROR, entryListener.getReturnedStatus());
    }
}
