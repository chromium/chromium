// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.HistoryContentManager.AppInfoCache;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;

import java.util.function.Supplier;

/** Unit tests for {@link HistoryItemView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HistoryItemViewUnitTest {
    private static final long TIMESTAMP_MS = 1_700_000_000_000L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AppInfoCache mAppInfoCache;
    @Mock private Callback<HistoryItem> mOnItemClicked;
    @Mock private Callback<HistoryItem> mOnItemRemoved;
    @Mock private Supplier<LargeIconBridge> mLargeIconBridgeSupplier;
    @Mock private LargeIconBridge mLargeIconBridge;

    @Captor private ArgumentCaptor<LargeIconCallback> mLargeIconCallbackCaptor;

    private HistoryItemView mItemView;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mItemView =
                (HistoryItemView)
                        LayoutInflater.from(activity).inflate(R.layout.history_item_view, null);
        mItemView.setSelectionDelegate(new SelectionDelegate<>());
        mItemView.setFaviconHelper(new DefaultFaviconHelper());
        mItemView.initialize(
                mAppInfoCache,
                () -> false,
                mOnItemClicked,
                mOnItemRemoved,
                mLargeIconBridgeSupplier);
    }

    @Test
    public void testHandleNonSelectionClick_InvokesCallbackWithBoundItem() {
        HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, TIMESTAMP_MS);
        mItemView.setItem(item);

        mItemView.handleNonSelectionClick();

        verify(mOnItemClicked).onResult(item);
    }

    @Test
    public void testHandleNonSelectionClick_NoOpWhenItemIsNull() {
        mItemView.handleNonSelectionClick();

        verify(mOnItemClicked, never()).onResult(any());
    }

    @Test
    public void testRemove_NoOpWhenItemIsNull() {
        mItemView.remove();

        verify(mOnItemRemoved, never()).onResult(any());
    }

    @Test
    public void testRemove_DoubleTapGuardBlocksSecondCall() {
        HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, TIMESTAMP_MS);
        mItemView.setItem(item);

        mItemView.remove();
        mItemView.remove();

        verify(mOnItemRemoved, times(1)).onResult(item);
    }

    @Test
    public void testRemove_RebindingItemResetsRemovedGuard() {
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, TIMESTAMP_MS);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, TIMESTAMP_MS);

        mItemView.setItem(item1);
        mItemView.remove();
        verify(mOnItemRemoved, times(1)).onResult(item1);

        // Re-binding a new item on a recycled view resets mIsItemRemoved.
        mItemView.setItem(item2);
        mItemView.remove();
        verify(mOnItemRemoved, times(1)).onResult(item2);
    }

    @Test
    public void testRequestIcon_FetchesAndUpdatesIconDrawable() {
        doReturn(mLargeIconBridge).when(mLargeIconBridgeSupplier).get();
        HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, TIMESTAMP_MS);

        mItemView.setItem(item);

        Drawable initialDrawable = mItemView.getIconDrawable();
        verify(mLargeIconBridge)
                .getLargeIconForUrl(
                        eq(item.getUrl()), anyInt(), mLargeIconCallbackCaptor.capture());

        Bitmap bitmap = Bitmap.createBitmap(16, 16, Bitmap.Config.ARGB_8888);
        mLargeIconCallbackCaptor
                .getValue()
                .onLargeIconAvailable(
                        bitmap, Color.BLUE, /* isFallbackColorDefault= */ false, IconType.FAVICON);

        assertNotNull(mItemView.getIconDrawable());
        assertNotSame(initialDrawable, mItemView.getIconDrawable());
    }

    @Test
    public void testRequestIcon_NullLargeIconBridgeSupplierNoOps() {
        doReturn(null).when(mLargeIconBridgeSupplier).get();
        HistoryItem item = StubbedHistoryProvider.createHistoryItem(0, TIMESTAMP_MS);

        mItemView.setItem(item);

        verify(mLargeIconBridgeSupplier).get();
    }

    @Test
    public void testRequestIcon_BlockedVisitSkipsBridge() {
        HistoryItem blockedItem =
                StubbedHistoryProvider.createHistoryItem(
                        5, TIMESTAMP_MS, /* blockedVisit= */ true, /* isActorVisit= */ false);

        mItemView.setItem(blockedItem);

        verify(mLargeIconBridgeSupplier, never()).get();
    }

    @Test
    public void testRequestIcon_IgnoresStaleCallbackAfterRebind() {
        doReturn(mLargeIconBridge).when(mLargeIconBridgeSupplier).get();
        HistoryItem item1 = StubbedHistoryProvider.createHistoryItem(0, TIMESTAMP_MS);
        HistoryItem item2 = StubbedHistoryProvider.createHistoryItem(1, TIMESTAMP_MS);

        mItemView.setItem(item1);
        verify(mLargeIconBridge)
                .getLargeIconForUrl(
                        eq(item1.getUrl()), anyInt(), mLargeIconCallbackCaptor.capture());
        LargeIconCallback staleCallback = mLargeIconCallbackCaptor.getValue();

        // Rebind view to item2 before item1's icon callback returns.
        mItemView.setItem(item2);
        verify(mLargeIconBridge)
                .getLargeIconForUrl(
                        eq(item2.getUrl()), anyInt(), mLargeIconCallbackCaptor.capture());
        LargeIconCallback activeCallback = mLargeIconCallbackCaptor.getValue();
        Drawable drawableAfterRebind = mItemView.getIconDrawable();

        // Stale callback for item1 must be ignored.
        Bitmap staleBitmap = Bitmap.createBitmap(16, 16, Bitmap.Config.ARGB_8888);
        staleCallback.onLargeIconAvailable(
                staleBitmap, Color.RED, /* isFallbackColorDefault= */ false, IconType.FAVICON);
        assertSame(drawableAfterRebind, mItemView.getIconDrawable());

        // Active callback for item2 still updates the drawable.
        Bitmap activeBitmap = Bitmap.createBitmap(16, 16, Bitmap.Config.ARGB_8888);
        activeCallback.onLargeIconAvailable(
                activeBitmap, Color.GREEN, /* isFallbackColorDefault= */ false, IconType.FAVICON);
        assertNotNull(mItemView.getIconDrawable());
        assertNotSame(drawableAfterRebind, mItemView.getIconDrawable());
    }
}
