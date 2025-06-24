// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnKeyListener;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewStub;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;

/**
 * Shows a popup of menu items anchored to a host view.
 *
 * <p>When an item is selected, we call {@link AppMenuHandlerImpl#onOptionsItemSelected}, which then
 * delegates to {@link AppMenuDelegate#onOptionsItemSelected}.
 *
 * <ul>
 *   <li>Only visible menu items are shown.
 *   <li>Disabled items are grayed out.
 * </ul>
 */
@NullMarked
class AppMenu implements OnKeyListener {

    /** Delegate to be notified of various visibility events from the app menu. */
    interface AppMenuVisibilityDelegate {
        /** Called when the AppMenu is dismissed. */
        void appMenuDismissed();

        /**
         * Called by AppMenu to report that the App Menu visibility has changed.
         *
         * @param isVisible Whether the App Menu is showing.
         */
        void onMenuVisibilityChanged(boolean isVisible);

        /**
         * A notification that the header view has been inflated.
         *
         * @param view The inflated view.
         */
        void onHeaderViewInflated(View view);

        /**
         * A notification that the footer view has been inflated.
         *
         * @param view The inflated view.
         */
        void onFooterViewInflated(View view);
    }

    /** Provides initial sizing information for the app menu. */
    interface InitialSizingHelper {
        /**
         * Get the preferred initial height for a given view.
         *
         * @param index The index of the view in the Adapter.
         * @return The recommended initial height for the view at a given index (in pixels).
         */
        int getInitialHeightForView(int index);

        /** Return whether the view at the given index can be the last initial view displayed. */
        boolean canBeLastVisibleInitialView(int index);
    }

    private static final float LAST_ITEM_SHOW_FRACTION = 0.5f;

    /** A means of reporting an exception/stack without crashing. */
    private static @MonotonicNonNull Callback<Throwable> sExceptionReporter;

    private final int mVerticalFadeDistance;
    private final int mNegativeSoftwareVerticalOffset;
    private final int mChipHighlightExtension;
    private final int[] mTempLocation;
    private final AppMenuVisibilityDelegate mVisibilityDelegate;

    private @Nullable PopupWindow mPopup;
    private @Nullable ListView mListView;
    private @Nullable ListAdapter mAdapter;
    private @Nullable View mFooterView;
    private int mCurrentScreenRotation = -1;
    private boolean mIsByPermanentButton;
    private @Nullable AnimatorSet mMenuItemEnterAnimator;
    private long mMenuShownTimeMs;
    private boolean mSelectedItemBeforeDismiss;
    private InitialSizingHelper mInitialSizingHelper;

    /**
     * Creates and sets up the App Menu.
     *
     * @param visibilityDelegate The visibility delegate for the Menu.
     * @param res Resources object used to get dimensions and style attributes.
     */
    AppMenu(AppMenuVisibilityDelegate visibilityDelegate, Resources res) {
        mVisibilityDelegate = visibilityDelegate;

        mNegativeSoftwareVerticalOffset =
                res.getDimensionPixelSize(R.dimen.menu_negative_software_vertical_offset);
        mVerticalFadeDistance = res.getDimensionPixelSize(R.dimen.menu_vertical_fade_distance);
        mChipHighlightExtension =
                res.getDimensionPixelOffset(R.dimen.menu_chip_highlight_extension);

        mTempLocation = new int[2];
    }

    /**
     * Creates and shows the app menu anchored to the specified view.
     *
     * @param context The context of the AppMenu (ensure the proper theme is set on this context).
     * @param anchorView The anchor {@link View} of the {@link PopupWindow}.
     * @param isByPermanentButton Whether or not permanent hardware button triggered it. (oppose to
     *     software button or keyboard).
     * @param screenRotation Current device screen rotation.
     * @param visibleDisplayFrame The display area rect in which AppMenu is supposed to fit in.
     * @param footerResourceId The resource id for a view to add as a fixed view at the bottom of
     *     the menu. Can be 0 if no such view is required. The footer is always visible and overlays
     *     other app menu items if necessary.
     * @param headerResourceId The resource id for a view to add as the first item in menu list. Can
     *     be null if no such view is required. See {@link ListView#addHeaderView(View)}.
     * @param highlightedItemId The resource id of the menu item that should be highlighted. Can be
     *     {@code null} if no item should be highlighted. Note that {@code 0} is dedicated to custom
     *     menu items and can be declared by external apps.
     * @param isMenuIconAtStart Whether the menu is being shown from a menu icon positioned at the
     *     start.
     * @param addTopPaddingBeforeFirstRow Whether top padding is needed above the first row.
     */
    void show(
            Context context,
            final View anchorView,
            boolean isByPermanentButton,
            int screenRotation,
            Rect visibleDisplayFrame,
            @IdRes int footerResourceId,
            @IdRes int headerResourceId,
            @Nullable Integer highlightedItemId,
            boolean isMenuIconAtStart,
            @ControlsPosition int controlsPosition,
            boolean addTopPaddingBeforeFirstRow) {
        mPopup = new PopupWindow(context);
        mPopup.setFocusable(true);
        mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);

        // The window layout type affects the z-index of the popup window.
        mPopup.setWindowLayoutType(WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL);

        mPopup.setOnDismissListener(
                () -> {
                    recordTimeToTakeActionHistogram();
                    if (anchorView instanceof ImageButton) {
                        ((ImageButton) anchorView).setSelected(false);
                    }

                    if (mMenuItemEnterAnimator != null) mMenuItemEnterAnimator.cancel();

                    mVisibilityDelegate.appMenuDismissed();
                    mVisibilityDelegate.onMenuVisibilityChanged(false);

                    mPopup = null;
                    mAdapter = null;
                    mListView = null;
                    mFooterView = null;
                    mMenuItemEnterAnimator = null;
                });

        // Some OEMs don't actually let us change the background... but they still return the
        // padding of the new background, which breaks the menu height.  If we still have a
        // drawable here even though our style says @null we should use this padding instead...
        Drawable originalBgDrawable = mPopup.getBackground();

        // Setting this to a transparent ColorDrawable instead of null because setting it to null
        // prevents the menu from being dismissed by tapping outside or pressing the back button on
        // Android L.
        mPopup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        // Make sure that the popup window will be closed when touch outside of it.
        mPopup.setOutsideTouchable(true);

        if (!isByPermanentButton) {
            mPopup.setAnimationStyle(
                    isMenuIconAtStart
                            ? R.style.StartIconMenuAnim
                            : (controlsPosition == ControlsPosition.TOP
                                    ? R.style.EndIconMenuAnim
                                    : R.style.EndIconMenuAnimBottom));
        }

        // Turn off window animations for low end devices.
        if (SysUtils.isLowEndDevice()) mPopup.setAnimationStyle(0);

        mCurrentScreenRotation = screenRotation;
        mIsByPermanentButton = isByPermanentButton;

        View contentView = createAppMenuContentView(context, addTopPaddingBeforeFirstRow);

        if (SysUtils.isLowEndDevice()) {
            var sharedDrawable = AppCompatResources.getDrawable(context, R.drawable.popup_bg_8dp);
            if (sharedDrawable != null) {
                var drawable = sharedDrawable.mutate();
                drawable.setTint(SemanticColorUtils.getMenuBgColor(context));
                drawable.setTintMode(PorterDuff.Mode.MULTIPLY);
                contentView.setBackground(drawable);
            }
        }

        Rect bgPadding = new Rect();
        contentView.getBackground().getPadding(bgPadding);

        int menuWidth = context.getResources().getDimensionPixelSize(R.dimen.menu_width);
        int popupWidth = menuWidth + bgPadding.left + bgPadding.right;

        mPopup.setWidth(popupWidth);

        Rect sizingPadding = new Rect(bgPadding);
        if (isByPermanentButton && originalBgDrawable != null) {
            Rect originalPadding = new Rect();
            originalBgDrawable.getPadding(originalPadding);
            sizingPadding.top = originalPadding.top;
            sizingPadding.bottom = originalPadding.bottom;
        }

        mListView = contentView.findViewById(R.id.app_menu_list);

        int footerHeight = inflateFooter(footerResourceId, contentView, menuWidth);
        int headerHeight = inflateHeader(headerResourceId, contentView, menuWidth);

        if (highlightedItemId != null) {
            View viewToHighlight = contentView.findViewById(highlightedItemId);
            HighlightParams highlightParams = new HighlightParams(HighlightShape.RECTANGLE);
            if (viewToHighlight instanceof ChipView) {
                ChipView chipViewToHighlight = (ChipView) viewToHighlight;
                highlightParams.setCornerRadius(chipViewToHighlight.getCornerRadius());
                highlightParams.setHighlightExtension(mChipHighlightExtension);
                // Set clip children and padding should be false to prevent the highlight from
                // getting clipped.
                ViewParent chipViewParent = chipViewToHighlight.getParent();
                if (chipViewParent instanceof ViewGroup) {
                    ViewGroup parentViewGroup = (ViewGroup) chipViewParent;
                    parentViewGroup.setClipToPadding(false);
                    parentViewGroup.setClipChildren(false);
                }
            }
            ViewHighlighter.turnOnHighlight(viewToHighlight, highlightParams);
        }

        // Set the adapter after the header is added to avoid crashes on JellyBean.
        // See crbug.com/761726.
        assert mAdapter != null;
        mListView.setAdapter(mAdapter);

        anchorView.getLocationOnScreen(mTempLocation);
        // getLocationOnScreen() may return incorrect location when anchorView is scrolled up and
        // leave the screen. In this case, we reset the location as 0 to indicate that the
        // anchorView is out of the visible screen area. See https://crbug.com/392698392.
        mTempLocation[1] = Math.max(mTempLocation[1], 0);

        int anchorViewOffset =
                Math.min(
                        Math.abs(mTempLocation[1] - visibleDisplayFrame.top),
                        Math.abs(mTempLocation[1] - visibleDisplayFrame.bottom));
        setMenuHeight(
                mInitialSizingHelper,
                visibleDisplayFrame,
                sizingPadding,
                footerHeight,
                headerHeight,
                anchorView,
                anchorViewOffset);
        int[] popupPosition =
                getPopupPosition(
                        mTempLocation,
                        mIsByPermanentButton,
                        mNegativeSoftwareVerticalOffset,
                        mCurrentScreenRotation,
                        visibleDisplayFrame,
                        sizingPadding,
                        anchorView,
                        popupWidth,
                        anchorView.getRootView().getLayoutDirection());
        mPopup.setContentView(contentView);

        try {
            mPopup.showAtLocation(
                    anchorView.getRootView(),
                    Gravity.NO_GRAVITY,
                    popupPosition[0],
                    popupPosition[1]);
        } catch (WindowManager.BadTokenException e) {
            // Intentionally ignore BadTokenException. This can happen in a real edge case where
            // parent.getWindowToken is not valid. See http://crbug.com/826052 &
            // https://crbug.com/1105831.
            return;
        }

        mSelectedItemBeforeDismiss = false;
        mMenuShownTimeMs = SystemClock.elapsedRealtime();

        mListView.setItemsCanFocus(true);
        mListView.setOnKeyListener(this);

        mVisibilityDelegate.onMenuVisibilityChanged(true);

        if (mVerticalFadeDistance > 0) {
            mListView.setVerticalFadingEdgeEnabled(true);
            mListView.setFadingEdgeLength(mVerticalFadeDistance);
        }

        // Don't animate the menu items for low end devices.
        if (!SysUtils.isLowEndDevice()) {
            mListView.addOnLayoutChangeListener(
                    new View.OnLayoutChangeListener() {
                        @Override
                        public void onLayoutChange(
                                View v,
                                int left,
                                int top,
                                int right,
                                int bottom,
                                int oldLeft,
                                int oldTop,
                                int oldRight,
                                int oldBottom) {
                            assumeNonNull(mListView);
                            mListView.removeOnLayoutChangeListener(this);
                            runMenuItemEnterAnimations();
                        }
                    });
        }
    }

    @VisibleForTesting
    static int[] getPopupPosition(
            int[] tempLocation,
            boolean isByPermanentButton,
            int negativeSoftwareVerticalOffset,
            int screenRotation,
            Rect appRect,
            Rect padding,
            View anchorView,
            int popupWidth,
            int viewLayoutDirection) {
        anchorView.getLocationInWindow(tempLocation);
        int anchorViewX = tempLocation[0];
        int anchorViewY = tempLocation[1];

        int[] offsets = new int[2];
        // If we have a hardware menu button, locate the app menu closer to the estimated
        // hardware menu button location.
        if (isByPermanentButton) {
            int horizontalOffset = -anchorViewX;
            switch (screenRotation) {
                case Surface.ROTATION_0:
                case Surface.ROTATION_180:
                    horizontalOffset += (appRect.width() - popupWidth) / 2;
                    break;
                case Surface.ROTATION_90:
                    horizontalOffset += appRect.width() - popupWidth;
                    break;
                case Surface.ROTATION_270:
                    break;
                default:
                    assert false;
                    break;
            }
            offsets[0] = horizontalOffset;
            // The menu is displayed above the anchored view, so shift the menu up by the bottom
            // padding of the background.
            offsets[1] = -padding.bottom;
        } else {
            offsets[1] = -negativeSoftwareVerticalOffset;
            if (viewLayoutDirection != View.LAYOUT_DIRECTION_RTL) {
                offsets[0] = anchorView.getWidth() - popupWidth;
            }
        }

        int xPos = anchorViewX + offsets[0];
        int yPos = anchorViewY + offsets[1];
        int[] position = {xPos, yPos};
        return position;
    }

    /** Marks whether an item was selected prior to dismissal. */
    public void setSelectedItemBeforeDismiss(boolean selected) {
        mSelectedItemBeforeDismiss = selected;
    }

    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        if (mListView == null) return false;
        if (event.getKeyCode() == KeyEvent.KEYCODE_MENU) {
            if (event.getAction() == KeyEvent.ACTION_DOWN && event.getRepeatCount() == 0) {
                event.startTracking();
                v.getKeyDispatcherState().startTracking(event, this);
                return true;
            } else if (event.getAction() == KeyEvent.ACTION_UP) {
                v.getKeyDispatcherState().handleUpEvent(event);
                if (event.isTracking() && !event.isCanceled()) {
                    dismiss();
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Update the menu items.
     *
     * @param sizingHelper The initial sizing helper for the menu.
     * @param adapter The adapter for visible items in the Menu.
     */
    @Initializer
    void updateMenu(InitialSizingHelper sizingHelper, ListAdapter adapter) {
        mInitialSizingHelper = sizingHelper;
        mAdapter = adapter;
    }

    /** Dismisses the app menu and cancels the drag-to-scroll if it is taking place. */
    void dismiss() {
        if (isShowing()) {
            mPopup.dismiss();
        }
    }

    /**
     * @return Whether the app menu is currently showing.
     */
    @EnsuresNonNullIf("mPopup")
    boolean isShowing() {
        if (mPopup == null) {
            return false;
        }
        return mPopup.isShowing();
    }

    /**
     * @return {@link PopupWindow} that displays all the menu options and optional footer.
     */
    @Nullable PopupWindow getPopup() {
        return mPopup;
    }

    /**
     * @return {@link ListView} that contains all of the menu options.
     */
    @Nullable ListView getListView() {
        return mListView;
    }

    @RequiresNonNull("mPopup")
    private void setMenuHeight(
            InitialSizingHelper sizingHelper,
            Rect appDimensions,
            Rect padding,
            int footerHeight,
            int headerHeight,
            View anchorView,
            int anchorViewOffset) {
        assert mAdapter != null;
        int anchorViewImpactHeight = mIsByPermanentButton ? anchorView.getHeight() : 0;

        int availableScreenSpace =
                appDimensions.height()
                        - anchorViewOffset
                        - padding.bottom
                        - footerHeight
                        - headerHeight
                        - anchorViewImpactHeight;

        if (mIsByPermanentButton) availableScreenSpace -= padding.top;
        if (availableScreenSpace <= 0 && sExceptionReporter != null) {
            String logMessage =
                    "there is no screen space for app menu, mIsByPermanentButton = "
                            + mIsByPermanentButton
                            + ", anchorViewOffset = "
                            + anchorViewOffset
                            + ", appDimensions.height() = "
                            + appDimensions.height()
                            + ", anchorView.getHeight() = "
                            + anchorView.getHeight()
                            + " padding.top = "
                            + padding.top
                            + ", padding.bottom = "
                            + padding.bottom
                            + ", footerHeight = "
                            + footerHeight
                            + ", headerHeight = "
                            + headerHeight;
            PostTask.postTask(
                    TaskTraits.BEST_EFFORT_MAY_BLOCK,
                    () -> sExceptionReporter.onResult(new Throwable(logMessage)));
        }

        // Find the height for each menu item.
        int[] heightList = new int[mAdapter.getCount()];
        boolean[] canBeLastList = new boolean[mAdapter.getCount()];
        for (int i = 0; i < mAdapter.getCount(); i++) {
            heightList[i] = sizingHelper.getInitialHeightForView(i);
            canBeLastList[i] = sizingHelper.canBeLastVisibleInitialView(i);
        }

        int menuHeight = calculateHeightForItems(heightList, canBeLastList, availableScreenSpace);
        menuHeight += footerHeight + headerHeight + padding.top + padding.bottom;
        mPopup.setHeight(menuHeight);
    }

    @VisibleForTesting
    static int calculateHeightForItems(
            int[] heightList, boolean[] canBeLastVisibleList, int screenSpaceForItems) {
        int availableScreenSpace = screenSpaceForItems > 0 ? screenSpaceForItems : 0;
        int spaceForFullItems = 0;

        assert heightList.length == canBeLastVisibleList.length;
        for (int height : heightList) {
            spaceForFullItems += height;
        }

        int menuHeight;
        // Fade out the last item if we cannot fit all items.
        if (availableScreenSpace < spaceForFullItems) {
            int spaceForItems = 0;
            int lastItem = 0;
            // App menu should show 1 full item at least.
            do {
                spaceForItems += heightList[lastItem++];
                if (spaceForItems + heightList[lastItem] > availableScreenSpace) {
                    break;
                }
            } while (lastItem < heightList.length - 1);

            int spaceForPartialItem = (int) (LAST_ITEM_SHOW_FRACTION * heightList[lastItem]);
            // Determine which item needs hiding. We only show Partial of the last item, if there is
            // not enough screen space to partially show the last identified item, then partially
            // show the second to last item instead. We also do not show the partial divider line.
            while (lastItem > 1
                    && (spaceForItems + spaceForPartialItem > availableScreenSpace
                            || !canBeLastVisibleList[lastItem])) {
                // If we have space for < 2.5 items, size menu to available screen space.
                if (spaceForItems <= availableScreenSpace && lastItem < 3) {
                    spaceForPartialItem = availableScreenSpace - spaceForItems;
                    break;
                }
                spaceForItems -= heightList[lastItem - 1];
                spaceForPartialItem = (int) (LAST_ITEM_SHOW_FRACTION * heightList[lastItem - 1]);
                lastItem--;
            }

            menuHeight = spaceForItems + spaceForPartialItem;
        } else {
            menuHeight = spaceForFullItems;
        }
        return menuHeight;
    }

    @RequiresNonNull("mListView")
    private void runMenuItemEnterAnimations() {
        mMenuItemEnterAnimator = new AnimatorSet();
        AnimatorSet.Builder builder = null;

        ViewGroup list = mListView;
        for (int i = 0; i < list.getChildCount(); i++) {
            View view = list.getChildAt(i);
            Object animatorObject = view.getTag(R.id.menu_item_enter_anim_id);
            if (animatorObject != null) {
                if (builder == null) {
                    builder = mMenuItemEnterAnimator.play((Animator) animatorObject);
                } else {
                    builder.with((Animator) animatorObject);
                }
            }
        }

        mMenuItemEnterAnimator.start();
    }

    private View createAppMenuContentView(Context context, boolean addTopPaddingBeforeFirstRow) {
        ViewGroup contentView =
                (ViewGroup) LayoutInflater.from(context).inflate(R.layout.app_menu_layout, null);
        if (addTopPaddingBeforeFirstRow) {
            contentView.setBackgroundResource(R.drawable.default_popup_menu_bg);
        } else {
            contentView.setBackgroundResource(R.drawable.app_menu_bottom_padding_bg);
        }
        return contentView;
    }

    private int inflateFooter(int footerResourceId, View contentView, int menuWidth) {
        if (footerResourceId == 0) {
            mFooterView = null;
            return 0;
        }

        ViewStub footerStub = contentView.findViewById(R.id.app_menu_footer_stub);
        footerStub.setLayoutResource(footerResourceId);
        mFooterView = footerStub.inflate();

        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(menuWidth, MeasureSpec.EXACTLY);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        mFooterView.measure(widthMeasureSpec, heightMeasureSpec);

        if (mVisibilityDelegate != null) mVisibilityDelegate.onFooterViewInflated(mFooterView);

        return mFooterView.getMeasuredHeight();
    }

    @RequiresNonNull("mListView")
    private int inflateHeader(int headerResourceId, View contentView, int menuWidth) {
        if (headerResourceId == 0) return 0;

        View headerView =
                LayoutInflater.from(contentView.getContext())
                        .inflate(headerResourceId, mListView, false);
        mListView.addHeaderView(headerView);

        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(menuWidth, MeasureSpec.EXACTLY);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        headerView.measure(widthMeasureSpec, heightMeasureSpec);

        if (mVisibilityDelegate != null) mVisibilityDelegate.onHeaderViewInflated(headerView);

        return headerView.getMeasuredHeight();
    }

    void finishAnimationsForTests() {
        if (mMenuItemEnterAnimator != null) mMenuItemEnterAnimator.end();
    }

    private void recordTimeToTakeActionHistogram() {
        final String histogramName =
                "Mobile.AppMenu.TimeToTakeAction."
                        + (mSelectedItemBeforeDismiss ? "SelectedItem" : "Abandoned");
        final long timeToTakeActionMs = SystemClock.elapsedRealtime() - mMenuShownTimeMs;
        RecordHistogram.deprecatedRecordMediumTimesHistogram(histogramName, timeToTakeActionMs);
    }

    /**
     * @param reporter A means of reporting an exception without crashing.
     */
    static void setExceptionReporter(Callback<Throwable> reporter) {
        sExceptionReporter = reporter;
    }
}
