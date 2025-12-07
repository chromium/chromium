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
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.ListAdapter;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.hierarchicalmenu.FlyoutController;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.FlyoutPopupSpecCalculator;
import org.chromium.ui.widget.RectProvider;

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

    /**
     * A data structure that holds the parameters and constraints for calculating the height of the
     * app menu.
     */
    static class MenuSpec {

        /** The display area rect in which AppMenu is supposed to fit in. */
        public final Rect visibleDisplayFrame;

        /** The padding to use for the menu. */
        public final Rect padding;

        /** The height of the footer. */
        public final int footerHeight;

        /** The height of the header. */
        public final int headerHeight;

        /** The anchor {@link View} of the menu. */
        public final View anchorView;

        /** Unusable space either above or below the anchor. */
        public final int anchorViewOffset;

        MenuSpec(
                Rect visibleDisplayFrame,
                Rect padding,
                int footerHeight,
                int headerHeight,
                View anchorView,
                int anchorViewOffset) {
            this.visibleDisplayFrame = visibleDisplayFrame;
            this.padding = padding;
            this.footerHeight = footerHeight;
            this.headerHeight = headerHeight;
            this.anchorView = anchorView;
            this.anchorViewOffset = anchorViewOffset;
        }
    }

    /**
     * A wrapper class that holds either a main {@link PopupWindow} or a flyout {@link
     * AnchoredPopupWindow}, to accommodate {@link FlyoutHandler}. TODO(crbug.com/454148603): Use
     * {@link AnchoredPopupWindow} for the main popup so that we can remove this wrapper.
     */
    static class AppMenuPopup {
        private final @Nullable PopupWindow mMainPopup;
        private final @Nullable AnchoredPopupWindow mFlyoutPopup;

        /**
         * Constructs an AppMenuPopup for a main popup window.
         *
         * @param popup The {@link PopupWindow} to wrap.
         */
        public AppMenuPopup(PopupWindow popup) {
            mMainPopup = popup;
            mFlyoutPopup = null;
        }

        /**
         * Constructs an AppMenuPopup for a flyout popup window.
         *
         * @param popup The {@link AnchoredPopupWindow} to wrap.
         */
        public AppMenuPopup(AnchoredPopupWindow popup) {
            mMainPopup = null;
            mFlyoutPopup = popup;
        }

        /** Dismisses the currently held popup window (either main or flyout). */
        public void dismiss() {
            assert mMainPopup != null || mFlyoutPopup != null;
            if (mMainPopup != null) {
                mMainPopup.dismiss();
            } else if (mFlyoutPopup != null) {
                mFlyoutPopup.dismiss();
            }
        }

        /**
         * Returns whether this wrapper holds the main popup or the flyout popup.
         *
         * @return {@code true} if main popup, {@code false} if flyout popup.
         */
        public boolean isMainPopup() {
            assert mMainPopup != null || mFlyoutPopup != null;
            return mMainPopup != null;
        }

        /**
         * Gets the main popup window.
         *
         * @return The main {@link Popupwindow}.
         */
        public PopupWindow getMainPopup() {
            assert isMainPopup();

            assert mMainPopup != null;
            return mMainPopup;
        }

        /**
         * Gets the {@link Rect} of the popup, relative to the application window.
         *
         * @return {@link Rect} of this popup.
         */
        public Rect getPopupRect() {
            View contentView = getContentView();
            assert contentView != null;

            Rect rootViewRect = new Rect();
            contentView.getRootView().getWindowVisibleDisplayFrame(rootViewRect);
            int[] viewCoordinates = new int[2];
            contentView.getLocationOnScreen(viewCoordinates);

            int left = viewCoordinates[0] - rootViewRect.left;
            int top = viewCoordinates[1] - rootViewRect.top;

            return new Rect(
                    left, top, left + contentView.getWidth(), top + contentView.getHeight());
        }

        /**
         * Gets the content view of the popup.
         *
         * @return The content view.
         */
        public @Nullable View getContentView() {
            assert mMainPopup != null || mFlyoutPopup != null;

            if (mMainPopup != null) {
                return mMainPopup.getContentView();
            } else if (mFlyoutPopup != null) {
                return mFlyoutPopup.getContentView();
            }

            return null;
        }
    }

    private static final float LAST_ITEM_SHOW_FRACTION = 0.5f;

    /** A means of reporting an exception/stack without crashing. */
    private static @MonotonicNonNull Callback<Throwable> sExceptionReporter;

    private final int mVerticalFadeDistance;
    private final int mNegativeSoftwareVerticalOffset;
    private final int mChipHighlightExtension;
    private final int[] mTempLocation;
    private final AppMenuVisibilityDelegate mVisibilityDelegate;
    private final boolean mDisableVerticalScrollbar;

    private @Nullable Context mContext;
    private @Nullable ListView mListView;
    private @Nullable ListAdapter mAdapter;
    private @Nullable View mFooterView;
    private int mCurrentScreenRotation = -1;
    private boolean mIsByPermanentButton;
    private @Nullable AnimatorSet mMenuItemEnterAnimator;
    private long mMenuShownTimeMs;
    private boolean mSelectedItemBeforeDismiss;
    private InitialSizingHelper mInitialSizingHelper;
    private @Nullable MenuSpec mMenuSpec;
    private final HierarchicalMenuController mHierarchicalMenuController;

    /**
     * Creates and sets up the App Menu.
     *
     * @param visibilityDelegate The visibility delegate for the Menu.
     * @param res Resources object used to get dimensions and style attributes.
     */
    AppMenu(
            AppMenuVisibilityDelegate visibilityDelegate,
            Resources res,
            HierarchicalMenuController hierarchicalMenuController,
            boolean disableVerticalScrollbar) {
        mVisibilityDelegate = visibilityDelegate;
        mDisableVerticalScrollbar = disableVerticalScrollbar;

        mNegativeSoftwareVerticalOffset =
                res.getDimensionPixelSize(R.dimen.menu_negative_software_vertical_offset);
        mVerticalFadeDistance = res.getDimensionPixelSize(R.dimen.menu_vertical_fade_distance);
        mChipHighlightExtension =
                res.getDimensionPixelOffset(R.dimen.menu_chip_highlight_extension);

        mTempLocation = new int[2];
        mHierarchicalMenuController = hierarchicalMenuController;
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
     * @param footer The view to add as a fixed view at the bottom of the menu. Can be null if no
     *     such view is required. The footer is always visible and overlays other app menu items if
     *     necessary.
     * @param header The resource id for a view to add as the first item in menu list. Can be null
     *     if no such view is required. See {@link ListView#addHeaderView(View)}.
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
            @Nullable View footer,
            @Nullable View header,
            @Nullable Integer highlightedItemId,
            boolean isMenuIconAtStart,
            @ControlsPosition int controlsPosition,
            boolean addTopPaddingBeforeFirstRow,
            FlyoutHandler flyoutHandler) {
        mContext = context;
        PopupWindow popup = new PopupWindow(context);
        popup.setFocusable(true);
        popup.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);

        // The window layout type affects the z-index of the popup window.
        popup.setWindowLayoutType(WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL);

        popup.setOnDismissListener(
                () -> {
                    recordTimeToTakeActionHistogram();
                    if (anchorView instanceof ImageButton) {
                        ((ImageButton) anchorView).setSelected(false);
                    }

                    if (mMenuItemEnterAnimator != null) mMenuItemEnterAnimator.cancel();

                    mVisibilityDelegate.appMenuDismissed();
                    mVisibilityDelegate.onMenuVisibilityChanged(false);

                    if (mHierarchicalMenuController.getFlyoutController() == null) {
                        return;
                    }
                    mHierarchicalMenuController.destroyFlyoutController();

                    mAdapter = null;
                    mListView = null;
                    mFooterView = null;
                    mMenuItemEnterAnimator = null;
                    mMenuSpec = null;
                });

        // Some OEMs don't actually let us change the background... but they still return the
        // padding of the new background, which breaks the menu height.  If we still have a
        // drawable here even though our style says @null we should use this padding instead...
        Drawable originalBgDrawable = popup.getBackground();

        // Setting this to a transparent ColorDrawable instead of null because setting it to null
        // prevents the menu from being dismissed by tapping outside or pressing the back button on
        // Android L.
        popup.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        // Make sure that the popup window will be closed when touch outside of it.
        popup.setOutsideTouchable(true);

        if (!isByPermanentButton) {
            popup.setAnimationStyle(
                    isMenuIconAtStart
                            ? R.style.StartIconMenuAnim
                            : (controlsPosition == ControlsPosition.TOP
                                    ? R.style.EndIconMenuAnim
                                    : R.style.EndIconMenuAnimBottom));
        }

        // Turn off window animations for low end devices.
        if (SysUtils.isLowEndDevice()) popup.setAnimationStyle(0);

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

        int menuWidth;
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)) {
            menuWidth = context.getResources().getDimensionPixelSize(R.dimen.menu_width_lff);
        } else {
            menuWidth = context.getResources().getDimensionPixelSize(R.dimen.menu_width);
        }
        int popupWidth = menuWidth + bgPadding.left + bgPadding.right;

        popup.setWidth(popupWidth);

        Rect padding = new Rect(bgPadding);
        if (isByPermanentButton && originalBgDrawable != null) {
            Rect originalPadding = new Rect();
            originalBgDrawable.getPadding(originalPadding);
            padding.top = originalPadding.top;
            padding.bottom = originalPadding.bottom;
        }

        mListView = contentView.findViewById(R.id.app_menu_list);
        if (mDisableVerticalScrollbar) {
            // TODO(crbug.com/465107697) Move code to xml file once the feature is launched.
            // Cleanup AppMenuDelegate too.
            mListView.setVerticalScrollBarEnabled(false);
        }

        int footerHeight = attachFooter(footer, (ViewGroup) contentView, menuWidth);
        int headerHeight = attachHeader(header, menuWidth);

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

        mMenuSpec =
                new MenuSpec(
                        visibleDisplayFrame,
                        padding,
                        footerHeight,
                        headerHeight,
                        anchorView,
                        anchorViewOffset);

        popup.setHeight(calculateMenuHeight());

        int[] popupPosition =
                getPopupPosition(
                        mTempLocation,
                        mIsByPermanentButton,
                        mNegativeSoftwareVerticalOffset,
                        mCurrentScreenRotation,
                        visibleDisplayFrame,
                        padding,
                        anchorView,
                        popupWidth,
                        anchorView.getRootView().getLayoutDirection());
        popup.setContentView(contentView);

        mHierarchicalMenuController.setupFlyoutController(
                /* flyoutHandler= */ flyoutHandler,
                new AppMenuPopup(popup),
                /* drillDownOverrideValue= */ null);

        // Post the show call to handle keyboard click events.
        if (ChromeFeatureList.sAndroidWebAppMenuButton.isEnabled()) {
            anchorView.post(
                    () -> {
                        showPopup(anchorView, popupPosition);
                    });
        } else {
            showPopup(anchorView, popupPosition);
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

    /**
     * Creates, displays, and tracks a new flyout sub-menu. This is called by {@link
     * AppMenuHandlerImpl} to fulfill the {@link FlyoutHandler} interface.
     *
     * @param adapter The {@link ListAdapter} containing the items to display in the new flyout.
     * @param view The menu item {@link View} that is triggering this flyout (used as the anchor).
     * @param item The {@link ListItem} model associated with the anchor {@code view}, used for
     *     tracking.
     * @param dismissRunnable The runnable to run after the window is dismissed.
     */
    public AppMenuPopup createAndShowFlyoutPopup(
            ListAdapter adapter, View view, ListItem item, Runnable dismissRunnable) {
        assert mContext != null;
        View contentView =
                createAppMenuContentView(mContext, /* addTopPaddingBeforeFirstRow= */ true);

        ListView listView = contentView.findViewById(R.id.app_menu_list);
        listView.setAdapter(adapter);
        listView.setItemsCanFocus(true);

        final int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
        int maxWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.menu_width) + lateralPadding;
        int menuWidth =
                UiUtils.computeListAdapterContentDimensions(adapter, listView)[0] + lateralPadding;

        assert mMenuSpec != null;
        AnchoredPopupWindow popup =
                new AnchoredPopupWindow.Builder(
                                view.getContext(),
                                mMenuSpec.anchorView.getRootView(),
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> contentView,
                                new RectProvider(
                                        FlyoutController.calculateFlyoutAnchorRect(
                                                view, mMenuSpec.anchorView.getRootView())))
                        .setVerticalOverlapAnchor(true)
                        .setHorizontalOverlapAnchor(false)
                        .setFocusable(true)
                        .setDesiredContentWidth(menuWidth)
                        .setMaxWidth(maxWidth)
                        .setTouchModal(false)
                        .setAnimateFromAnchor(false)
                        .setAnimationStyle(R.style.PopupWindowAnimFade)
                        .setSpecCalculator(new FlyoutPopupSpecCalculator())
                        .setWindowLayoutType(WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL)
                        .addOnDismissListener(
                                () -> {
                                    dismissRunnable.run();
                                })
                        .build();

        popup.show();
        return new AppMenuPopup(popup);
    }

    void setContentDescription(@Nullable String desc) {
        PopupWindow mainPopup = getPopup();
        if (mainPopup == null) return;

        View contentView = mainPopup.getContentView();
        if (contentView == null) return;

        contentView.setAccessibilityLiveRegion(
                desc != null
                        ? View.ACCESSIBILITY_LIVE_REGION_POLITE
                        : View.ACCESSIBILITY_LIVE_REGION_NONE);
        contentView.setContentDescription(desc);
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
        if (mHierarchicalMenuController.getFlyoutController() != null) {
            mHierarchicalMenuController.destroyFlyoutController();
        }
    }

    /**
     * @return Whether the app menu is currently showing.
     */
    boolean isShowing() {
        PopupWindow mainPopup = getPopup();
        return mainPopup != null ? mainPopup.isShowing() : false;
    }

    /**
     * @return {@link PopupWindow} that displays all the menu options and optional footer.
     */
    @Nullable PopupWindow getPopup() {
        FlyoutController<AppMenuPopup> controller =
                mHierarchicalMenuController.getFlyoutController();
        if (controller == null) {
            return null;
        }

        return controller.getMainPopup().getMainPopup();
    }

    /**
     * @return {@link ListView} that contains all of the menu options.
     */
    @Nullable ListView getListView() {
        return mListView;
    }

    /** Recalculates and updates the height of the popup window while it is showing. */
    public void updateMenuHeight() {
        PopupWindow mainPopup = getPopup();
        assert mainPopup != null && mainPopup.isShowing();
        mainPopup.update(mainPopup.getWidth(), calculateMenuHeight());
    }

    private int calculateMenuHeight() {
        assert mAdapter != null;

        if (mInitialSizingHelper == null || mMenuSpec == null) {
            return 0;
        }

        int anchorViewImpactHeight = mIsByPermanentButton ? mMenuSpec.anchorView.getHeight() : 0;

        int availableScreenSpace =
                mMenuSpec.visibleDisplayFrame.height()
                        - mMenuSpec.anchorViewOffset
                        - mMenuSpec.padding.bottom
                        - mMenuSpec.footerHeight
                        - mMenuSpec.headerHeight
                        - anchorViewImpactHeight;

        if (mIsByPermanentButton) availableScreenSpace -= mMenuSpec.padding.top;
        if (availableScreenSpace <= 0 && sExceptionReporter != null) {
            String logMessage =
                    "there is no screen space for app menu, mIsByPermanentButton = "
                            + mIsByPermanentButton
                            + ", anchorViewOffset = "
                            + mMenuSpec.anchorViewOffset
                            + ", visibleDisplayFrame.height() = "
                            + mMenuSpec.visibleDisplayFrame.height()
                            + ", anchorView.getHeight() = "
                            + mMenuSpec.anchorView.getHeight()
                            + ", padding.top = "
                            + mMenuSpec.padding.top
                            + ", padding.bottom = "
                            + mMenuSpec.padding.bottom
                            + ", footerHeight = "
                            + mMenuSpec.footerHeight
                            + ", headerHeight = "
                            + mMenuSpec.headerHeight;
            PostTask.postTask(
                    TaskTraits.BEST_EFFORT_MAY_BLOCK,
                    () -> sExceptionReporter.onResult(new Throwable(logMessage)));
        }

        // Find the height for each menu item.
        int itemCount = mAdapter == null ? 0 : mAdapter.getCount();
        int[] heightList = new int[itemCount];
        boolean[] canBeLastList = new boolean[itemCount];
        for (int i = 0; i < itemCount; i++) {
            heightList[i] = mInitialSizingHelper.getInitialHeightForView(i);
            canBeLastList[i] = mInitialSizingHelper.canBeLastVisibleInitialView(i);
        }

        int menuHeight = calculateHeightForItems(heightList, canBeLastList, availableScreenSpace);
        menuHeight +=
                mMenuSpec.footerHeight
                        + mMenuSpec.headerHeight
                        + mMenuSpec.padding.top
                        + mMenuSpec.padding.bottom;

        return menuHeight;
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

    private int attachFooter(@Nullable View footer, ViewGroup contentView, int menuWidth) {
        if (footer == null) {
            mFooterView = null;
            return 0;
        }

        mFooterView = footer;
        mFooterView.setId(R.id.app_menu_footer);
        contentView.addView(
                footer, contentView.indexOfChild(contentView.findViewById(R.id.app_menu_list)) + 1);

        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(menuWidth, MeasureSpec.EXACTLY);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        mFooterView.measure(widthMeasureSpec, heightMeasureSpec);

        return mFooterView.getMeasuredHeight();
    }

    @RequiresNonNull("mListView")
    private int attachHeader(@Nullable View header, int menuWidth) {
        if (header == null) return 0;

        mListView.addHeaderView(header);

        int widthMeasureSpec = MeasureSpec.makeMeasureSpec(menuWidth, MeasureSpec.EXACTLY);
        int heightMeasureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        header.measure(widthMeasureSpec, heightMeasureSpec);

        return header.getMeasuredHeight();
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

    private void showPopup(View anchorView, int[] popupPosition) {
        PopupWindow mainPopup = getPopup();

        if (mainPopup == null) return;
        try {
            mainPopup.showAtLocation(
                    anchorView.getRootView(),
                    Gravity.NO_GRAVITY,
                    popupPosition[0],
                    popupPosition[1]);
        } catch (WindowManager.BadTokenException e) {
            // Intentionally ignore BadTokenException. This can happen in a real
            // edge case where parent.getWindowToken is not valid. See
            // http://crbug.com/826052 & https://crbug.com/1105831.
            return;
        }
    }
}
