// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.net.Uri;
import android.os.Build;
import android.util.Pair;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.View;
import android.view.View.OnCreateContextMenuListener;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtilsJni;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lens.LensController;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.share.LensUtils;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowAndroid.OnCloseContextMenuListener;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * A helper class that handles generating context menus for {@link WebContents}s.
 */
public class ContextMenuHelper implements OnCreateContextMenuListener {
    public static Callback<RevampedContextMenuCoordinator> sRevampedContextMenuShownCallback;

    private final WebContents mWebContents;
    private long mNativeContextMenuHelper;

    private ContextMenuPopulator mPopulator;
    private ContextMenuParams mCurrentContextMenuParams;
    private WindowAndroid mWindow;
    private Activity mActivity;
    private Callback<Integer> mCallback;
    private Runnable mOnMenuShown;
    private Callback<Boolean> mOnMenuClosed;
    private long mMenuShownTimeMs;
    private boolean mSelectedItemBeforeDismiss;

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
        if (mPopulator != null) mPopulator.onDestroy();
        mNativeContextMenuHelper = 0;
    }

    /**
     * @param populator A {@link ContextMenuPopulator} that is responsible for managing and showing
     *                  context menus.
     */
    @CalledByNative
    private void setPopulator(ContextMenuPopulator populator) {
        if (mPopulator != null) mPopulator.onDestroy();
        mPopulator = populator;
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
                || mPopulator == null) {
            return;
        }

        mCurrentContextMenuParams = params;
        mWindow = windowAndroid;
        mActivity = windowAndroid.getActivity().get();
        mCallback = (result) -> {
            mSelectedItemBeforeDismiss = true;
            mPopulator.onItemSelected(mCurrentContextMenuParams, renderFrameHost, result);
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
            mPopulator.onMenuClosed();
            if (LensUtils.enableShoppyImageMenuItem() || LensUtils.enableImageChip()) {
                // If the image was being classified terminate the classification
                // Has no effect if the classification already succeeded.
                LensController.getInstance().terminateClassification();
            }

            if (mNativeContextMenuHelper == 0) return;
            ContextMenuHelperJni.get().onContextMenuClosed(
                    mNativeContextMenuHelper, ContextMenuHelper.this);
        };

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.REVAMPED_CONTEXT_MENU)
                && params.getSourceType() != MenuSourceType.MENU_SOURCE_MOUSE) {
            // NOTE: This is a temporary implementation to enable experimentation and should not
            // not be enabled under any circumstances on Stable Chrome builds due to potential
            // latency impact.
            if (LensUtils.enableShoppyImageMenuItem()
                    && LensController.getInstance().isSdkAvailable()) {
                mPopulator.retrieveImage(
                        renderFrameHost, ContextMenuImageFormat.ORIGINAL, (Uri uri) -> {
                            LensController.getInstance().classifyImage(
                                    uri, (Boolean isShoppyImage) -> {
                                        displayRevampedContextMenu(
                                                renderFrameHost, topContentOffsetPx, isShoppyImage);
                                    });
                        });
            } else {
                displayRevampedContextMenu(
                        renderFrameHost, topContentOffsetPx, /* addShoppyMenuItem */ false);
            }
            return;
        }

        // The Platform Context Menu requires the listener within this helper since this helper and
        // provides context menu for us to show.
        view.setOnCreateContextMenuListener(this);
        boolean wasContextMenuShown = false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                && params.getSourceType() == MenuSourceType.MENU_SOURCE_MOUSE) {
            final float density = view.getResources().getDisplayMetrics().density;
            final float touchPointXPx = params.getTriggeringTouchXDp() * density;
            final float touchPointYPx =
                    (params.getTriggeringTouchYDp() * density) + topContentOffsetPx;
            wasContextMenuShown = view.showContextMenu(touchPointXPx, touchPointYPx);
        } else {
            wasContextMenuShown = view.showContextMenu();
        }
        if (wasContextMenuShown) {
            mOnMenuShown.run();
            windowAndroid.addContextMenuCloseListener(new OnCloseContextMenuListener() {
                @Override
                public void onContextMenuClosed() {
                    mOnMenuClosed.onResult(false);
                    windowAndroid.removeContextMenuCloseListener(this);
                }
            });
        }
    }

    private void displayRevampedContextMenu(
            RenderFrameHost renderFrameHost, float topContentOffsetPx, boolean addShoppyMenuItem) {
        List<Pair<Integer, List<ContextMenuItem>>> items = mPopulator.buildContextMenu(
                null, mActivity, mCurrentContextMenuParams, addShoppyMenuItem);
        if (items.isEmpty()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mOnMenuClosed.bind(false));
            return;
        }

        final RevampedContextMenuCoordinator menuCoordinator = new RevampedContextMenuCoordinator(
                topContentOffsetPx, () -> shareImageWithLastShareComponent(renderFrameHost));

        if (LensUtils.enableImageChip()) {
            LensAsyncManager lensAsyncManager =
                    new LensAsyncManager(mCurrentContextMenuParams, mPopulator, renderFrameHost);
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
            mPopulator.getThumbnail(
                    renderFrameHost, menuCoordinator.getOnImageThumbnailRetrievedReference());
        }
    }

    /**
     * Share the image that triggered the current context menu with the last app used to share.
     * @param renderFrameHost {@link RenderFrameHost} to get the encoded images from.
     */
    private void shareImageWithLastShareComponent(RenderFrameHost renderFrameHost) {
        mPopulator.retrieveImage(renderFrameHost, ContextMenuImageFormat.ORIGINAL, (Uri uri) -> {
            ShareHelper.shareImage(mWindow, ShareHelper.getLastShareComponentName(), uri);
        });
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        List<Pair<Integer, List<ContextMenuItem>>> items =
                mPopulator.buildContextMenu(menu, v.getContext(), mCurrentContextMenuParams, false);

        if (items.isEmpty()) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mOnMenuClosed.bind(false));
            return;
        }
        ContextMenuUi menuUi = new PlatformContextMenuUi(menu);
        menuUi.displayMenu(mWindow, mWebContents, mCurrentContextMenuParams, items, mCallback,
                mOnMenuShown, mOnMenuClosed);
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

    /**
     * @return The {@link ContextMenuPopulator} responsible for populating the context menu.
     */
    @VisibleForTesting
    public ContextMenuPopulator getPopulator() {
        return mPopulator;
    }

    @NativeMethods
    interface Natives {
        void onContextMenuClosed(long nativeContextMenuHelper, ContextMenuHelper caller);
    }
}
