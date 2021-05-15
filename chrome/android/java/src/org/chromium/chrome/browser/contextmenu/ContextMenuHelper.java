// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.util.Pair;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtilsJni;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * A helper class that handles generating context menus for {@link WebContents}s.
 */
public class ContextMenuHelper {
    private static Callback<RevampedContextMenuCoordinator> sMenuShownCallbackForTests;

    private final WebContents mWebContents;
    private long mNativeContextMenuHelper;

    private ContextMenuNativeDelegate mCurrentNativeDelegate;
    private ContextMenuPopulator mCurrentPopulator;
    private ContextMenuPopulatorFactory mPopulatorFactory;
    private ContextMenuParams mCurrentContextMenuParams;
    private ContextMenuUi mCurrentContextMenu;
    private WindowAndroid mWindow;
    private Callback<Integer> mCallback;
    private Runnable mOnMenuShown;
    private Runnable mOnMenuClosed;
    private long mMenuShownTimeMs;
    private boolean mSelectedItemBeforeDismiss;
    private boolean mIsIncognito;
    private String mPageTitle;
    private ChipDelegate mChipDelegate;

    private ContextMenuHelper(long nativeContextMenuHelper, WebContents webContents) {
        mNativeContextMenuHelper = nativeContextMenuHelper;
        mWebContents = webContents;
    }

    @CalledByNative
    private static ContextMenuHelper create(long nativeContextMenuHelper, WebContents webContents) {
        return new ContextMenuHelper(nativeContextMenuHelper, webContents);
    }

    @CalledByNative
    private void destroy() {
        if (mCurrentContextMenu != null) {
            mCurrentContextMenu.dismiss();
            mCurrentContextMenu = null;
        }
        if (mCurrentNativeDelegate != null) mCurrentNativeDelegate.destroy();
        if (mPopulatorFactory != null) mPopulatorFactory.onDestroy();
        mNativeContextMenuHelper = 0;
    }

    @CalledByNative
    private void setPopulatorFactory(ContextMenuPopulatorFactory populatorFactory) {
        if (mCurrentContextMenu != null) {
            mCurrentContextMenu.dismiss();
            mCurrentContextMenu = null;
        }
        if (mCurrentNativeDelegate != null) mCurrentNativeDelegate.destroy();
        mCurrentPopulator = null;
        if (mPopulatorFactory != null) mPopulatorFactory.onDestroy();
        mPopulatorFactory = populatorFactory;
    }

    /**
     * Starts showing a context menu for {@code view} based on {@code params}.
     * @param params The {@link ContextMenuParams} that indicate what menu items to show.
     * @param renderFrameHost {@link RenderFrameHost} to get the encoded images from.
     * @param view container view for the menu.
     * @param topContentOffsetPx the offset of the content from the top.
     */
    @CalledByNative
    private void showContextMenu(final ContextMenuParams params, RenderFrameHost renderFrameHost,
            View view, float topContentOffsetPx) {
        if (params.isFile()) return;
        final WindowAndroid windowAndroid = mWebContents.getTopLevelNativeWindow();

        if (view == null || view.getVisibility() != View.VISIBLE || view.getParent() == null
                || windowAndroid == null || windowAndroid.getActivity().get() == null
                || mPopulatorFactory == null || mCurrentContextMenu != null) {
            return;
        }

        mCurrentNativeDelegate =
                new ContextMenuNativeDelegateImpl(mWebContents, renderFrameHost, params);
        mCurrentPopulator = mPopulatorFactory.createContextMenuPopulator(
                windowAndroid.getActivity().get(), params, mCurrentNativeDelegate);
        mIsIncognito = mCurrentPopulator.isIncognito();
        mPageTitle = mCurrentPopulator.getPageTitle();
        mCurrentContextMenuParams = params;
        mWindow = windowAndroid;
        mCallback = (result) -> {
            if (mCurrentPopulator == null) return;

            mSelectedItemBeforeDismiss = true;
            mCurrentPopulator.onItemSelected(result);
        };
        mOnMenuShown = () -> {
            mSelectedItemBeforeDismiss = false;
            mMenuShownTimeMs =
                    TimeUnit.MICROSECONDS.toMillis(TimeUtilsJni.get().getTimeTicksNowUs());
            RecordHistogram.recordBooleanHistogram("ContextMenu.Shown", mWebContents != null);
            if (LensUtils.isInShoppingAllowlist(mCurrentContextMenuParams.getPageUrl())) {
                RecordHistogram.recordBooleanHistogram(
                        "ContextMenu.Shown.ShoppingDomain", mWebContents != null);
            }
        };
        mOnMenuClosed = () -> {
            recordTimeToTakeActionHistogram(mSelectedItemBeforeDismiss);
            mCurrentContextMenu = null;
            if (mCurrentNativeDelegate != null) {
                mCurrentNativeDelegate.destroy();
                mCurrentNativeDelegate = null;
            }
            if (mCurrentPopulator != null) {
                mCurrentPopulator.onMenuClosed();
                mCurrentPopulator = null;
            }
            if (mChipDelegate != null) {
                // If the image was being classified terminate the classification
                // Has no effect if the classification already succeeded.
                mChipDelegate.onMenuClosed();
            }
            if (mNativeContextMenuHelper == 0) return;
            ContextMenuHelperJni.get().onContextMenuClosed(
                    mNativeContextMenuHelper, ContextMenuHelper.this);
        };

        displayRevampedContextMenu(topContentOffsetPx);
    }

    private void displayRevampedContextMenu(float topContentOffsetPx) {
        List<Pair<Integer, ModelList>> items = mCurrentPopulator.buildContextMenu();
        if (items.isEmpty()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mOnMenuClosed);
            if (sMenuShownCallbackForTests != null) {
                sMenuShownCallbackForTests.onResult(null);
            }
            return;
        }

        final RevampedContextMenuCoordinator menuCoordinator =
                new RevampedContextMenuCoordinator(topContentOffsetPx, mCurrentNativeDelegate);
        mCurrentContextMenu = menuCoordinator;
        mChipDelegate = mCurrentPopulator.getChipDelegate();

        if (mChipDelegate != null) {
            menuCoordinator.displayMenuWithChip(mWindow, mWebContents, mCurrentContextMenuParams,
                    items, mCallback, mOnMenuShown, mOnMenuClosed, mChipDelegate);
        } else {
            menuCoordinator.displayMenu(mWindow, mWebContents, mCurrentContextMenuParams, items,
                    mCallback, mOnMenuShown, mOnMenuClosed);
        }

        if (sMenuShownCallbackForTests != null) {
            sMenuShownCallbackForTests.onResult(menuCoordinator);
        }
    }

    private void recordTimeToTakeActionHistogram(boolean selectedItem) {
        final String histogramName =
                "ContextMenu.TimeToTakeAction." + (selectedItem ? "SelectedItem" : "Abandoned");
        final long timeToTakeActionMs =
                TimeUnit.MICROSECONDS.toMillis(TimeUtilsJni.get().getTimeTicksNowUs())
                - mMenuShownTimeMs;
        RecordHistogram.recordTimesHistogram(histogramName, timeToTakeActionMs);
        if (mCurrentContextMenuParams.isAnchor()
                && PerformanceHintsObserver.getPerformanceClassForURL(
                           mWebContents, mCurrentContextMenuParams.getLinkUrl())
                        == PerformanceHintsObserver.PerformanceClass.PERFORMANCE_FAST) {
            RecordHistogram.recordTimesHistogram(
                    histogramName + ".PerformanceClassFast", timeToTakeActionMs);
        }
    }

    @VisibleForTesting
    public static void setMenuShownCallbackForTests(
            Callback<RevampedContextMenuCoordinator> callback) {
        sMenuShownCallbackForTests = callback;
    }

    @NativeMethods
    interface Natives {
        void onContextMenuClosed(long nativeContextMenuHelper, ContextMenuHelper caller);
    }
}
