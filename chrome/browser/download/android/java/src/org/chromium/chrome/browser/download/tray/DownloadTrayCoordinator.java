// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
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
 * anchored to the toolbar button, mirroring the desktop download bubble.
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

    /** Opens or closes the full tray; see {@link DownloadTrayMediator#toggleFullTray()}. */
    public void toggleFullTray() {
        mMediator.toggleFullTray();
    }

    // DownloadTrayMediator.PopupController implementation:

    @Override
    public boolean showCompletions(View anchor, ModelList rows) {
        if (!showPopup(
                anchor,
                themedContext -> buildList(themedContext, R.layout.download_tray_popup, rows),
                /* focusable= */ false)) {
            return false;
        }
        // Leave the popup up for screen reader users, who need time to find it, as desktop does.
        if (!AccessibilityState.isTouchExplorationEnabled()) {
            mHandler.postDelayed(mAutoDismissRunnable, AUTO_DISMISS_MS);
        }
        return true;
    }

    @Override
    public boolean showFullTray(View anchor, ModelList rows) {
        return showPopup(
                anchor, themedContext -> buildFullTray(themedContext, rows), /* focusable= */ true);
    }

    /**
     * Shows a popup with {@code content} anchored to {@code anchor}, replacing any shown popup.
     *
     * @param focusable Whether the popup takes focus, for keyboard access and Escape to close it.
     *     Only for popups the user opened, so one opening on its own leaves keystrokes to the page.
     */
    private boolean showPopup(
            View anchor, AnchoredPopupWindow.ViewBuilder content, boolean focusable) {
        dismiss();
        ViewRectProvider anchorRectProvider = new ViewRectProvider(anchor);
        mPopup =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                anchor.getRootView(),
                                assumeNonNull(mContext.getDrawable(R.drawable.menu_bg_tinted)),
                                content,
                                anchorRectProvider)
                        .setDesiredContentWidth(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.download_tray_popup_width))
                        .setPreferredVerticalOrientation(
                                AnchoredPopupWindow.VerticalOrientation.BELOW)
                        .setFocusable(focusable)
                        // Only taps outside close it, as for a menu; taps inside are the content's.
                        .setOutsideTouchable(true)
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
        return true;
    }

    @Override
    public void dismiss() {
        if (mPopup != null) mPopup.dismiss();
    }

    /** Inflates {@code layout}, which contains the tray's list, and binds {@code rows} to it. */
    private View buildList(Context themedContext, @LayoutRes int layout, ModelList rows) {
        View content = LayoutInflater.from(themedContext).inflate(layout, /* root= */ null);
        RecyclerView list = content.findViewById(R.id.download_tray_list);
        list.setLayoutManager(new LinearLayoutManager(themedContext));
        // The adapter keeps each row's view bound to its model while the popup shows.
        mAdapter = new SimpleRecyclerViewAdapter(rows);
        mAdapter.registerType(RowType.COMPLETED, this::buildRow, DownloadTrayItemViewBinder::bind);
        list.setAdapter(mAdapter);
        return content;
    }

    private View buildFullTray(Context themedContext, ModelList rows) {
        View content = buildList(themedContext, R.layout.download_tray_full_tray, rows);
        content.findViewById(R.id.download_tray_close).setOnClickListener(v -> dismiss());
        content.findViewById(R.id.download_tray_full_history)
                .setOnClickListener(v -> mMediator.onFullHistoryClicked());
        return content;
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
