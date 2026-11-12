// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.RowType;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * Entry point for the download toolbar button tray. Shows a popup listing completed downloads,
 * anchored to the toolbar button, mirroring the desktop download bubble's partial view.
 */
@NullMarked
public class DownloadTrayCoordinator implements DownloadTrayMediator.PopupController, Destroyable {
    /**
     * How long the completion popup stays before closing by itself. Mirrors desktop's {@code
     * kAutoClosePartialViewDelay}.
     */
    @VisibleForTesting static final long AUTO_DISMISS_MS = TimeUnit.SECONDS.toMillis(5);

    private final Context mContext;
    private final DownloadTrayMediator mMediator;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mAutoDismissRunnable = this::dismiss;

    private @Nullable AnchoredPopupWindow mPopup;
    private @Nullable SimpleRecyclerViewAdapter mAdapter;

    /**
     * @param context The context used to inflate the tray.
     * @param model The model to observe for download state changes.
     * @param anchorSupplier Supplies the view to anchor popups to, or null if unavailable.
     * @param isWindowFocused Whether this is the window the user last focused; only that window
     *     shows popups.
     * @param isIncognitoSelected Whether this window is showing incognito tabs.
     * @param onActioned Invoked when the user acts on a download from the tray.
     * @param delegate Performs actions that need dependencies outside the download module.
     */
    public DownloadTrayCoordinator(
            Context context,
            DownloadStateModel model,
            Supplier<@Nullable View> anchorSupplier,
            BooleanSupplier isWindowFocused,
            BooleanSupplier isIncognitoSelected,
            Runnable onActioned,
            DownloadTrayDelegate delegate) {
        mContext = context;
        mMediator =
                new DownloadTrayMediator(
                        context,
                        model,
                        anchorSupplier,
                        isWindowFocused,
                        isIncognitoSelected,
                        onActioned,
                        delegate,
                        /* popupController= */ this);
    }

    @Override
    public void destroy() {
        // The mediator also dismisses any popup it showed.
        mMediator.destroy();
    }

    // DownloadTrayMediator.PopupController implementation:

    @Override
    public boolean showCompletions(View anchor, ModelList rows) {
        dismiss();
        ViewRectProvider anchorRectProvider = new ViewRectProvider(anchor);
        Resources resources = mContext.getResources();
        mPopup =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                anchor.getRootView(),
                                assumeNonNull(mContext.getDrawable(R.drawable.menu_bg_tinted)),
                                themedContext -> buildList(themedContext, rows),
                                anchorRectProvider)
                        .setDesiredContentWidth(
                                resources.getDimensionPixelSize(R.dimen.download_tray_popup_width))
                        // Longer lists scroll, as desktop's row list does (kMaxHeightForRowList).
                        .setMaxHeight(
                                resources.getDimensionPixelSize(
                                        R.dimen.download_tray_popup_max_height))
                        .setPreferredVerticalOrientation(
                                AnchoredPopupWindow.VerticalOrientation.BELOW)
                        // Outside taps dismiss; taps on the row are handled by the row itself.
                        .setDismissOnTouchInteraction(true)
                        .setDismissOnScreenSizeChange(true)
                        .addOnDismissListener(this::onPopupDismissed)
                        .build();
        mPopup.show();
        if (!mPopup.isShowing()) {
            // AnchoredPopupWindow skips popups too small to interact with (e.g. a very short
            // window), but show() has already started observing the window and the anchor, and its
            // dismiss listener never runs. Stop the observers it lets us reach and reset state.
            mPopup.dismiss();
            anchorRectProvider.stopObserving();
            onPopupDismissed();
            return false;
        }

        // Leave the popup up for screen reader users, who need time to find it, as desktop does.
        if (!AccessibilityState.isTouchExplorationEnabled()) {
            mHandler.postDelayed(mAutoDismissRunnable, AUTO_DISMISS_MS);
        }
        return true;
    }

    @Override
    public void dismiss() {
        if (mPopup != null) mPopup.dismiss();
    }

    private View buildList(Context themedContext, ModelList rows) {
        RecyclerView list =
                (RecyclerView)
                        LayoutInflater.from(themedContext)
                                .inflate(R.layout.download_tray_popup, /* root= */ null);
        list.setLayoutManager(new LinearLayoutManager(themedContext));
        // The adapter keeps each row's view bound to its model while the popup shows.
        mAdapter = new SimpleRecyclerViewAdapter(rows);
        mAdapter.registerType(RowType.COMPLETED, this::buildRow, DownloadTrayItemViewBinder::bind);
        list.setAdapter(mAdapter);
        return list;
    }

    private View buildRow(ViewGroup parent) {
        View row =
                LayoutInflater.from(parent.getContext())
                        .inflate(
                                R.layout.download_tray_item_row, parent, /* attachToRoot= */ false);
        // As on desktop (DownloadBubblePartialView::OnMouseEntered), once the mouse enters the
        // popup it stays until dismissed, so it doesn't close under a pointer about to click it.
        // Rows consume hover events, so the listener goes on each row rather than the list.
        row.setOnHoverListener(
                (v, event) -> {
                    if (event.getActionMasked() == MotionEvent.ACTION_HOVER_ENTER) {
                        mHandler.removeCallbacks(mAutoDismissRunnable);
                    }
                    return false;
                });
        return row;
    }

    private void onPopupDismissed() {
        mHandler.removeCallbacks(mAutoDismissRunnable);
        if (mAdapter != null) {
            mAdapter.destroy();
            mAdapter = null;
        }
        mPopup = null;
        mMediator.onDismissed();
    }

    @Nullable AnchoredPopupWindow getPopupForTesting() {
        return mPopup;
    }
}
