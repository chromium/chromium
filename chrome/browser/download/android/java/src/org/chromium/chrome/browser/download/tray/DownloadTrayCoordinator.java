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

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;

import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * Entry point for the download toolbar button tray. Shows a popup anchored to the toolbar button
 * when a download completes, mirroring the desktop download bubble's partial view.
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
    private @Nullable PropertyModelChangeProcessor<PropertyModel, View, PropertyKey>
            mRowChangeProcessor;

    /**
     * @param context The context used to inflate the tray.
     * @param model The model to observe for download state changes.
     * @param anchorSupplier Supplies the view to anchor popups to, or null if unavailable.
     * @param isWindowFocused Whether this window has focus; only the focused window shows popups.
     * @param onActioned Invoked when the user acts on a download from the tray.
     * @param delegate Performs actions that need dependencies outside the download module.
     */
    public DownloadTrayCoordinator(
            Context context,
            DownloadStateModel model,
            Supplier<@Nullable View> anchorSupplier,
            BooleanSupplier isWindowFocused,
            Runnable onActioned,
            DownloadTrayDelegate delegate) {
        mContext = context;
        mMediator =
                new DownloadTrayMediator(
                        context,
                        model,
                        anchorSupplier,
                        isWindowFocused,
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
    public boolean showCompletion(View anchor, PropertyModel row) {
        dismiss();
        ViewRectProvider anchorRectProvider = new ViewRectProvider(anchor);
        mPopup =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                anchor.getRootView(),
                                assumeNonNull(mContext.getDrawable(R.drawable.menu_bg_tinted)),
                                themedContext -> buildRow(themedContext, row),
                                anchorRectProvider)
                        .setDesiredContentWidth(
                                mContext.getResources()
                                        .getDimensionPixelSize(R.dimen.download_tray_popup_width))
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

    private View buildRow(Context themedContext, PropertyModel row) {
        View view =
                LayoutInflater.from(themedContext)
                        .inflate(R.layout.download_tray_item_row, /* root= */ null);
        mRowChangeProcessor =
                PropertyModelChangeProcessor.create(row, view, DownloadTrayItemViewBinder::bind);
        // As on desktop (DownloadBubblePartialView::OnMouseEntered), once the mouse enters the
        // popup it stays until dismissed, so it doesn't close under a pointer about to click it.
        view.setOnHoverListener(
                (v, event) -> {
                    if (event.getActionMasked() == MotionEvent.ACTION_HOVER_ENTER) {
                        mHandler.removeCallbacks(mAutoDismissRunnable);
                    }
                    return false;
                });
        return view;
    }

    private void onPopupDismissed() {
        mHandler.removeCallbacks(mAutoDismissRunnable);
        if (mRowChangeProcessor != null) {
            mRowChangeProcessor.destroy();
            mRowChangeProcessor = null;
        }
        mPopup = null;
        mMediator.onDismissed();
    }

    @Nullable AnchoredPopupWindow getPopupForTesting() {
        return mPopup;
    }
}
