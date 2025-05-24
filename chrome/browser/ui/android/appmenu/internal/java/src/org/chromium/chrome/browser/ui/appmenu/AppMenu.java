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
import android.text.TextUtils;
import android.util.SparseArray;
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
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.PopupWindow;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

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
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;

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
class AppMenu implements OnItemClickListener, OnKeyListener, AppMenuClickHandler {
    private static final float LAST_ITEM_SHOW_FRACTION = 0.5f;

    /** A means of reporting an exception/stack without crashing. */
    private static @MonotonicNonNull Callback<Throwable> sExceptionReporter;

    private final int mItemRowHeight;
    private final int mVerticalFadeDistance;
    private final int mNegativeSoftwareVerticalOffset;
    private final int mChipHighlightExtension;
    private final int[] mTempLocation;

    private @Nullable PopupWindow mPopup;
    private @Nullable ListView mListView;
    private @Nullable ModelListAdapter mAdapter;
    private final AppMenuHandlerImpl mHandler;
    private @Nullable View mFooterView;
    private int mCurrentScreenRotation = -1;
    private boolean mIsByPermanentButton;
    private @Nullable AnimatorSet mMenuItemEnterAnimator;
    private long mMenuShownTimeMs;
    private boolean mSelectedItemBeforeDismiss;
    private ModelList mModelList;

    /**
     * Creates and sets up the App Menu.
     * @param itemRowHeight Desired height for each app menu row.
     * @param handler AppMenuHandlerImpl receives callbacks from AppMenu.
     * @param res Resources object used to get dimensions and style attributes.
     */
    AppMenu(int itemRowHeight, AppMenuHandlerImpl handler, Resources res) {
        mItemRowHeight = itemRowHeight;
        assert mItemRowHeight > 0;

        mHandler = handler;

        mNegativeSoftwareVerticalOffset =
                res.getDimensionPixelSize(R.dimen.menu_negative_software_vertical_offset);
        mVerticalFadeDistance = res.getDimensionPixelSize(R.dimen.menu_vertical_fade_distance);
        mChipHighlightExtension =
                res.getDimensionPixelOffset(R.dimen.menu_chip_highlight_extension);

        mTempLocation = new int[2];
    }

    /**
     * Notifies the menu that the contents of the menu item specified by {@code menuRowId} have
     * changed. This should be called if icons, titles, etc. are changing for a particular menu item
     * while the menu is open.
     *
     * @param menuRowId The id of the menu item to change. This must be a row id and not a child id.
     */
    public void menuItemContentChanged(int menuRowId) {
        // Make sure we have all the valid state objects we need.
        if (mAdapter == null || mPopup == null || mListView == null) {
            return;
        }

        // Calculate the item index.
        int index = -1;
        int menuSize = mModelList.size();
        for (int i = 0; i < menuSize; i++) {
            if (mModelList.get(i).model.get(AppMenuItemProperties.MENU_ITEM_ID) == menuRowId) {
                index = i;
                break;
            }
        }
        if (index == -1) return;

        // Check if the item is visible.
        int startIndex = mListView.getFirstVisiblePosition();
        int endIndex = mListView.getLastVisiblePosition();
        if (index < startIndex || index > endIndex) return;

        // Grab the correct View.
        View view = mListView.getChildAt(index - startIndex);
        if (view == null) return;

        // Cause the Adapter to re-populate the View.
        mAdapter.getView(index, view, mListView);
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
     * @param groupDividerResourceId The resource id of divider menu items. This will be used to
     *     determine the number of dividers that appear in the menu.
     * @param highlightedItemId The resource id of the menu item that should be highlighted. Can be
     *     {@code null} if no item should be highlighted. Note that {@code 0} is dedicated to custom
     *     menu items and can be declared by external apps.
     * @param customSizingProviders Provides sizing/height for item types that do not use the
     *     default item height. If a item's type does not have an entry in this object, then it
     *     should use the default sizing value.
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
            @IdRes int groupDividerResourceId,
            @Nullable Integer highlightedItemId,
            SparseArray<Function<Context, Integer>> customSizingProviders,
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

                    mHandler.appMenuDismissed();
                    mHandler.onMenuVisibilityChanged(false);

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

        // Find the height for each menu item.
        List<Integer> menuItemIds = new ArrayList<Integer>();
        List<Integer> heightList = new ArrayList<Integer>();
        for (int i = 0; i < mModelList.size(); i++) {
            int itemId = mModelList.get(i).model.get(AppMenuItemProperties.MENU_ITEM_ID);
            menuItemIds.add(itemId);
            heightList.add(
                    getMenuItemHeight(mModelList.get(i).type, context, customSizingProviders));
        }

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
                menuItemIds,
                heightList,
                visibleDisplayFrame,
                sizingPadding,
                footerHeight,
                headerHeight,
                anchorView,
                groupDividerResourceId,
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

        mListView.setOnItemClickListener(this);
        mListView.setItemsCanFocus(true);
        mListView.setOnKeyListener(this);

        mHandler.onMenuVisibilityChanged(true);

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

    @Override
    public void onItemClick(PropertyModel model, @Nullable MotionEventInfo triggeringMotion) {
        if (!model.get(AppMenuItemProperties.ENABLED)) return;

        int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
        mSelectedItemBeforeDismiss = true;
        dismiss();
        mHandler.onOptionsItemSelected(id, triggeringMotion);
    }

    @Override
    public boolean onItemLongClick(PropertyModel model, View view) {
        if (!model.get(AppMenuItemProperties.ENABLED)) return false;

        mSelectedItemBeforeDismiss = true;
        CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
        CharSequence message =
                TextUtils.isEmpty(titleCondensed)
                        ? model.get(AppMenuItemProperties.TITLE)
                        : titleCondensed;
        return showToastForItem(message, view);
    }

    @VisibleForTesting
    boolean showToastForItem(CharSequence message, View view) {
        Context context = view.getContext();
        final @ColorInt int backgroundColor = ContextCompat.getColor(context, R.color.toast_color);
        return new Toast.Builder(context)
                .withText(message)
                .withAnchoredView(view)
                .withBackgroundColor(backgroundColor)
                .withTextAppearance(R.style.TextAppearance_TextSmall_Primary)
                .buildAndShow();
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        onItemClick(mModelList.get(position).model);
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
     * @param newModelList The new menu item list will be displayed.
     * @param adapter The adapter for visible items in the Menu.
     */
    @Initializer
    void updateMenu(ModelList newModelList, ModelListAdapter adapter) {
        mModelList = newModelList;
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

    /**
     * @return The menu instance inside of this class.
     */
    ModelList getMenuModelList() {
        return mModelList;
    }

    /**
     * Find the {@link PropertyModel} associated with the given id. If the menu item is not found,
     * return null.
     * @param itemId The id of the menu item to find.
     * @return The {@link PropertyModel} has the given id. null if not found.
     */
    @Nullable PropertyModel getMenuItemPropertyModel(int itemId) {
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel model = mModelList.get(i).model;
            if (model.get(AppMenuItemProperties.MENU_ITEM_ID) == itemId) {
                return model;
            } else if (model.get(AppMenuItemProperties.ADDITIONAL_ICONS) != null) {
                ModelList subList = model.get(AppMenuItemProperties.ADDITIONAL_ICONS);
                for (int j = 0; j < subList.size(); j++) {
                    PropertyModel subModel = subList.get(j).model;
                    if (subModel.get(AppMenuItemProperties.MENU_ITEM_ID) == itemId) {
                        return subModel;
                    }
                }
            }
        }
        return null;
    }

    /** Invalidate the app menu data. See {@link AppMenuAdapter#notifyDataSetChanged}. */
    void invalidate() {
        if (mAdapter != null) mAdapter.notifyDataSetChanged();
    }

    @RequiresNonNull("mPopup")
    private void setMenuHeight(
            List<Integer> menuItemIds,
            List<Integer> heightList,
            Rect appDimensions,
            Rect padding,
            int footerHeight,
            int headerHeight,
            View anchorView,
            @IdRes int groupDividerResourceId,
            int anchorViewOffset) {
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
                    "there is no screen space for app menn, mIsByPermanentButton = "
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

        int menuHeight =
                calculateHeightForItems(
                        menuItemIds, heightList, groupDividerResourceId, availableScreenSpace);
        menuHeight += footerHeight + headerHeight + padding.top + padding.bottom;
        mPopup.setHeight(menuHeight);
    }

    @VisibleForTesting
    int calculateHeightForItems(
            List<Integer> menuItemIds,
            List<Integer> heightList,
            @IdRes int groupDividerResourceId,
            int screenSpaceForItems) {
        int availableScreenSpace = screenSpaceForItems > 0 ? screenSpaceForItems : 0;
        int spaceForFullItems = 0;

        for (int i = 0; i < heightList.size(); i++) {
            spaceForFullItems += heightList.get(i);
        }

        int menuHeight;
        // Fade out the last item if we cannot fit all items.
        if (availableScreenSpace < spaceForFullItems) {
            int spaceForItems = 0;
            int lastItem = 0;
            // App menu should show 1 full item at least.
            do {
                spaceForItems += heightList.get(lastItem++);
                if (spaceForItems + heightList.get(lastItem) > availableScreenSpace) {
                    break;
                }
            } while (lastItem < heightList.size() - 1);

            int spaceForPartialItem = (int) (LAST_ITEM_SHOW_FRACTION * heightList.get(lastItem));
            // Determine which item needs hiding. We only show Partial of the last item, if there is
            // not enough screen space to partially show the last identified item, then partially
            // show the second to last item instead. We also do not show the partial divider line.
            assert menuItemIds.size() == heightList.size();
            while (lastItem > 1
                    && (spaceForItems + spaceForPartialItem > availableScreenSpace
                            || menuItemIds.get(lastItem) == groupDividerResourceId)) {
                // If we have space for < 2.5 items, size menu to available screen space.
                if (spaceForItems <= availableScreenSpace && lastItem < 3) {
                    spaceForPartialItem = availableScreenSpace - spaceForItems;
                    break;
                }
                spaceForItems -= heightList.get(lastItem - 1);
                spaceForPartialItem =
                        (int) (LAST_ITEM_SHOW_FRACTION * heightList.get(lastItem - 1));
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

        if (mHandler != null) mHandler.onFooterViewInflated(mFooterView);

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

        if (mHandler != null) mHandler.onHeaderViewInflated(headerView);

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

    private int getMenuItemHeight(
            int itemType,
            Context context,
            SparseArray<Function<Context, Integer>> customSizingProviders) {
        if (customSizingProviders.get(itemType) != null) {
            return assumeNonNull(customSizingProviders.get(itemType)).apply(context);
        }
        return mItemRowHeight;
    }

    /**
     * @param reporter A means of reporting an exception without crashing.
     */
    static void setExceptionReporter(Callback<Throwable> reporter) {
        sExceptionReporter = reporter;
    }
}
