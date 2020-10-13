// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.net.Uri;
import android.util.Pair;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtilsJni;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.chrome.browser.share.ShareHelper;
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
    public static Callback<RevampedContextMenuCoordinator> sRevampedContextMenuShownCallback;

    private final WebContents mWebContents;
    private long mNativeContextMenuHelper;

    private ContextMenuPopulator mCurrentPopulator;
    private ContextMenuPopulatorFactory mPopulatorFactory;
    private ContextMenuParams mCurrentContextMenuParams;
    private WindowAndroid mWindow;
    private Callback<Integer> mCallback;
    private Runnable mOnMenuShown;
    private Callback<Boolean> mOnMenuClosed;
    private long mMenuShownTimeMs;
    private boolean mSelectedItemBeforeDismiss;
    private boolean mIsIncognito;

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
        if (mCurrentPopulator != null) mCurrentPopulator.onDestroy();
        if (mPopulatorFactory != null) mPopulatorFactory.onDestroy();
        mNativeContextMenuHelper = 0;
    }

    @CalledByNative
    private void setPopulatorFactory(ContextMenuPopulatorFactory populatorFactory) {
        if (mCurrentPopulator != null) mCurrentPopulator.onDestroy();
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
                || mPopulatorFactory == null) {
            return;
        }

        mCurrentPopulator = mPopulatorFactory.createContextMenuPopulator(
                windowAndroid.getActivity().get(), params, renderFrameHost);
        mIsIncognito = mCurrentPopulator.isIncognito();
        mCurrentContextMenuParams = params;
        mWindow = windowAndroid;
        mCallback = (result) -> {
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
        mOnMenuClosed = (notAbandoned) -> {
            recordTimeToTakeActionHistogram(mSelectedItemBeforeDismiss || notAbandoned);
            if (mCurrentPopulator != null) {
                mCurrentPopulator.onMenuClosed();
                mCurrentPopulator.onDestroy();
                mCurrentPopulator = null;
            }
            if (LensUtils.enableShoppyImageMenuItem() || LensUtils.enableImageChip(mIsIncognito)) {
                // If the image was being classified terminate the classification
                // Has no effect if the classification already succeeded.
                LensController.getInstance().terminateClassification();
            }
            if (mNativeContextMenuHelper == 0) return;
            ContextMenuHelperJni.get().onContextMenuClosed(
                    mNativeContextMenuHelper, ContextMenuHelper.this);
        };

        // NOTE: This is a temporary implementation to enable experimentation and should not
        // not be enabled under any circumstances on Stable Chrome builds due to potential
        // latency impact.
        if (LensUtils.enableShoppyImageMenuItem()) {
            mCurrentPopulator.retrieveImage(ContextMenuImageFormat.ORIGINAL, (Uri uri) -> {
                LensController.getInstance().classifyImage(uri, (Boolean isShoppyImage) -> {
                    displayRevampedContextMenu(topContentOffsetPx, isShoppyImage);
                });
            });
        } else {
            displayRevampedContextMenu(topContentOffsetPx, /* addShoppyMenuItem */ false);
        }
    }

    private void displayRevampedContextMenu(float topContentOffsetPx, boolean addShoppyMenuItem) {
        List<Pair<Integer, ModelList>> items =
                mCurrentPopulator.buildContextMenu(addShoppyMenuItem);
        if (items.isEmpty()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mOnMenuClosed.bind(false));
            return;
        }

        final RevampedContextMenuCoordinator menuCoordinator = new RevampedContextMenuCoordinator(
                topContentOffsetPx, () -> shareImageWithLastShareComponent());

        if (LensUtils.enableImageChip(mIsIncognito)) {
            LensAsyncManager lensAsyncManager =
                    new LensAsyncManager(mCurrentContextMenuParams, mCurrentPopulator);
            menuCoordinator.displayMenuWithLensChip(mWindow, mWebContents,
                    mCurrentContextMenuParams, items, mCallback, mOnMenuShown, mOnMenuClosed,
                    lensAsyncManager);
        } else {
            menuCoordinator.displayMenu(mWindow, mWebContents, mCurrentContextMenuParams, items,
                    mCallback, mOnMenuShown, mOnMenuClosed);
        }

        if (sRevampedContextMenuShownCallback != null) {
            sRevampedContextMenuShownCallback.onResult(menuCoordinator);
        }
        // TODO(sinansahin): This could be pushed in to the header mediator.
        if (mCurrentContextMenuParams.isImage()) {
            mCurrentPopulator.getThumbnail(menuCoordinator.getOnImageThumbnailRetrievedReference());
        }
    }

    /**
     * Share the image that triggered the current context menu with the last app used to share.
     */
    private void shareImageWithLastShareComponent() {
        mCurrentPopulator.retrieveImage(ContextMenuImageFormat.ORIGINAL, (Uri uri) -> {
            ShareHelper.shareImage(mWindow, ShareHelper.getLastShareComponentName(), uri);
        });
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

    @NativeMethods
    interface Natives {
        void onContextMenuClosed(long nativeContextMenuHelper, ContextMenuHelper caller);
    }
}
