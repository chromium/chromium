// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.widget.PopupWindow.OnDismissListener;

import androidx.annotation.IdRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.ViewCompat;

import org.chromium.base.Callback;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Shows the popup explaining that history isn't saved in Incognito, anchored near the top-end of
 * the window like Desktop's Incognito history disclaimer bubble. It's shown instead of the History
 * page when the user tries to open History from Incognito. One instance exists per window and is
 * registered on that window's {@link WindowAndroid}.
 */
@NullMarked
public class IncognitoHistoryDisclaimerCoordinator {
    private static final UnownedUserDataKey<IncognitoHistoryDisclaimerCoordinator> KEY =
            new UnownedUserDataKey<>((coordinator, host) -> coordinator.destroy());

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final @IdRes int[] mAnchorViewIds;
    private final OnGlobalLayoutListener mDeferredShowListener = this::onDeferredShowLayout;
    private final Callback<TabModel> mCurrentTabModelObserver = this::onCurrentTabModelChanged;
    private final OnDismissListener mOnPopupDismissed = this::onPopupDismissed;
    private @Nullable AnchoredPopupWindow mPopupWindow;
    private @Nullable MonotonicObservableSupplier<TabModel> mObservedTabModelSupplier;
    // The decor view waiting for its first layout before the popup can show, if any.
    private @Nullable View mDeferredShowView;
    private boolean mDestroyed;

    /**
     * Creates the coordinator and registers it on {@code windowAndroid}. The popup anchors to the
     * first shown view in {@code anchorViewIds}, or to the top-end of the content view if none is
     * shown.
     *
     * @param activity The {@link Activity} hosting the window.
     * @param windowAndroid The {@link WindowAndroid} this coordinator is registered on.
     * @param anchorViewIds View IDs to anchor the popup to, in priority order.
     */
    public IncognitoHistoryDisclaimerCoordinator(
            Activity activity, WindowAndroid windowAndroid, @IdRes int[] anchorViewIds) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mAnchorViewIds = anchorViewIds;
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), this);
    }

    /**
     * Shows the popup in {@code window}. Does nothing if the window has no coordinator.
     *
     * @param window The {@link WindowAndroid} to show the popup in.
     */
    public static void showForWindow(@Nullable WindowAndroid window) {
        if (window == null) return;
        IncognitoHistoryDisclaimerCoordinator coordinator =
                KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
        if (coordinator != null) coordinator.show();
    }

    /** Returns the coordinator registered on {@code window}, or null if there isn't one. */
    public static @Nullable IncognitoHistoryDisclaimerCoordinator getForWindowForTesting(
            WindowAndroid window) {
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    /**
     * Shows the popup. Does nothing if it's already showing or the window's current tab model isn't
     * Incognito.
     */
    public void show() {
        if (mDestroyed || isShowing() || mActivity.isFinishing() || mActivity.isDestroyed()) {
            return;
        }
        TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
        if (selector == null || !selector.isIncognitoBrandedModelSelected()) return;

        View decorView = mActivity.getWindow().getDecorView();
        if (!decorView.isLaidOut()) {
            // The window can get a History trigger before its first layout, e.g. a warmed-up
            // Incognito CCT loading a URL. Wait once for the next layout pass.
            if (mDeferredShowView == null) {
                mDeferredShowView = decorView;
                decorView.getViewTreeObserver().addOnGlobalLayoutListener(mDeferredShowListener);
            }
            return;
        }

        Context themedContext =
                new ContextThemeWrapper(
                        mActivity, R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        Drawable background =
                AppCompatResources.getDrawable(themedContext, R.drawable.menu_bg_tinted);
        assert background != null;
        Resources resources = mActivity.getResources();
        int desiredWidth =
                resources.getDimensionPixelSize(R.dimen.incognito_history_disclaimer_popup_width);
        int marginPx =
                getPopupMarginPx(
                        background,
                        resources.getDimensionPixelSize(R.dimen.menu_horizontal_margin));

        AnchoredPopupWindow popupWindow =
                new AnchoredPopupWindow.Builder(
                                mActivity,
                                decorView,
                                background,
                                context -> createContentView(themedContext),
                                createAnchorRectProvider(decorView))
                        .setFocusable(true)
                        .setOutsideTouchable(true)
                        .setTouchModal(true)
                        .setDismissOnScreenSizeChange(true)
                        .setPreferredVerticalOrientation(VerticalOrientation.BELOW)
                        // The anchor is at the top-end, so MAX_AVAILABLE_SPACE opens the card
                        // toward the start edge. Overlapping the anchor lines the card's end up
                        // with the anchor's end, like Desktop's bubble.
                        .setPreferredHorizontalOrientation(
                                HorizontalOrientation.MAX_AVAILABLE_SPACE)
                        .setHorizontalOverlapAnchor(true)
                        .setDesiredContentWidth(desiredWidth)
                        // Also caps the width so the card fits between both window margins.
                        .setMargin(marginPx)
                        .setElevation(0f)
                        .build();
        popupWindow.addOnDismissListener(mOnPopupDismissed);
        popupWindow.show();
        if (!popupWindow.isShowing()) return;

        mPopupWindow = popupWindow;
        mObservedTabModelSupplier = selector.getCurrentTabModelSupplier();
        mObservedTabModelSupplier.addSyncObserver(mCurrentTabModelObserver);
    }

    /** Dismisses the popup and unregisters this coordinator from its window. */
    public void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        cancelDeferredShow();
        stopObservingTabModel();
        dismiss();
        KEY.detachFromAllHosts(this);
    }

    /** Returns whether the popup is showing. */
    public boolean isShowing() {
        return mPopupWindow != null && mPopupWindow.isShowing();
    }

    /** Returns whether a show is waiting for the window's next layout. */
    boolean hasPendingShowForTesting() {
        return mDeferredShowView != null;
    }

    /** Returns the popup's content view, or null if the popup isn't showing. */
    @Nullable View getContentViewForTesting() {
        return mPopupWindow == null ? null : mPopupWindow.getContentView();
    }

    /**
     * Returns the margin to pass to {@link AnchoredPopupWindow} so the visible card, not its
     * background's shadow inset, stays {@code visibleEdgeMarginPx} from the window edges.
     */
    static int getPopupMarginPx(Drawable background, int visibleEdgeMarginPx) {
        Rect padding = new Rect();
        background.getPadding(padding);
        return Math.max(0, visibleEdgeMarginPx - Math.max(padding.left, padding.right));
    }

    /**
     * Returns the {@link RectProvider} the popup anchors to: the first shown view in {@link
     * #mAnchorViewIds}, or a point at the top-end corner of the content view.
     */
    RectProvider createAnchorRectProvider(View decorView) {
        View anchor = findAnchorView(decorView, mAnchorViewIds);
        if (anchor != null) return new ViewRectProvider(anchor);

        View content = mActivity.findViewById(android.R.id.content);
        int[] location = new int[2];
        content.getLocationOnScreen(location);
        boolean isRtl = content.getLayoutDirection() == View.LAYOUT_DIRECTION_RTL;
        int x = isRtl ? location[0] : location[0] + content.getWidth();
        return new RectProvider(new Rect(x, location[1], x, location[1]));
    }

    /** Returns the first view with one of {@code ids} that's shown under {@code root}. */
    static @Nullable View findAnchorView(View root, @IdRes int[] ids) {
        for (@IdRes int id : ids) {
            View view = findFirstShownView(root, id);
            if (view != null) return view;
        }
        return null;
    }

    // Copied from BookmarkUtils. Prunes subtrees that aren't VISIBLE, so a VISIBLE view inside a
    // hidden toolbar (e.g. while Hub is showing) isn't used.
    private static @Nullable View findFirstShownView(@Nullable View root, @IdRes int id) {
        if (root == null || root.getVisibility() != View.VISIBLE) return null;
        if (root.getId() == id && root.isShown()) return root;
        if (root instanceof ViewGroup viewGroup) {
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                View found = findFirstShownView(viewGroup.getChildAt(i), id);
                if (found != null) return found;
            }
        }
        return null;
    }

    private View createContentView(Context context) {
        View root =
                LayoutInflater.from(context)
                        .inflate(R.layout.incognito_history_disclaimer_popup, /* root= */ null);
        ViewCompat.setAccessibilityPaneTitle(
                root, context.getString(R.string.incognito_history_bubble_primary_text));
        root.findViewById(R.id.history_disclaimer_got_it).setOnClickListener(v -> dismiss());
        root.findViewById(R.id.history_disclaimer_close)
                .setOnClickListener(v -> onCloseIncognitoClicked());
        return root;
    }

    private void onDeferredShowLayout() {
        cancelDeferredShow();
        show();
    }

    private void cancelDeferredShow() {
        if (mDeferredShowView == null) return;
        mDeferredShowView.getViewTreeObserver().removeOnGlobalLayoutListener(mDeferredShowListener);
        mDeferredShowView = null;
    }

    private void onCloseIncognitoClicked() {
        // Dismiss first: closing the last tab can finish an Incognito window synchronously.
        dismiss();
        TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
        if (selector == null) return;
        selector.getModel(/* incognito= */ true)
                .getTabRemover()
                .closeTabs(TabClosureParams.closeAllTabs().build(), /* allowDialog= */ false);
    }

    private void onCurrentTabModelChanged(TabModel tabModel) {
        if (!tabModel.isIncognitoBranded()) dismiss();
    }

    private void dismiss() {
        if (mPopupWindow != null) mPopupWindow.dismiss();
    }

    private void onPopupDismissed() {
        stopObservingTabModel();
        mPopupWindow = null;
    }

    private void stopObservingTabModel() {
        if (mObservedTabModelSupplier == null) return;
        mObservedTabModelSupplier.removeObserver(mCurrentTabModelObserver);
        mObservedTabModelSupplier = null;
    }
}
