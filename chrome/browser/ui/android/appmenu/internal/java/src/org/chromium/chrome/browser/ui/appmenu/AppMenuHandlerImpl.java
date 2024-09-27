// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.Point;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;
import android.view.Display;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Object responsible for handling the creation, showing, hiding of the AppMenu and notifying the
 * AppMenuObservers about these actions.
 */
class AppMenuHandlerImpl
        implements AppMenuHandler, StartStopWithNativeObserver, ConfigurationChangedObserver {
    private AppMenu mAppMenu;
    private AppMenuDragHelper mAppMenuDragHelper;
    private final List<AppMenuBlocker> mBlockers;
    private final List<AppMenuObserver> mObservers;
    private final View mHardwareButtonMenuAnchor;

    private final Context mContext;
    private final AppMenuPropertiesDelegate mDelegate;
    private final AppMenuDelegate mAppMenuDelegate;
    private final View mDecorView;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final Supplier<Rect> mAppRect;
    private final WindowAndroid mWindowAndroid;
    private ModelList mModelList;
    private ListObserver<Void> mListObserver;
    private Callback<Integer> mTestOptionsItemSelectedListener;
    private KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;

    /**
     * The resource id of the menu item to highlight when the menu next opens. A value of {@code
     * null} means no item will be highlighted. This value will be cleared after the menu is opened.
     */
    private Integer mHighlightMenuId;

    /**
     * Constructs an AppMenuHandlerImpl object.
     * @param context The activity context.
     * @param delegate Delegate used to check the desired AppMenu properties on show.
     * @param appMenuDelegate The AppMenuDelegate to handle menu item selection.
     * @param menuResourceId Resource Id that should be used as the source for the menu items. It is
     *     assumed to have back_menu_id, forward_menu_id, bookmark_this_page_id.
     * @param decorView The decor {@link View}, e.g. from Window#getDecorView(), for the containing
     *     activity.
     * @param activityLifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the containing
     *     activity.
     * @param hardwareButtonAnchorView The {@link View} used as an anchor for the menu when it is
     *     displayed using a hardware button.
     * @param appRect Supplier of the app area in Window that the menu should fit in.
     * @param windowAndroid The window that will be used to fetch {@link KeyboardVisibilityDelegate}
     */
    public AppMenuHandlerImpl(
            Context context,
            AppMenuPropertiesDelegate delegate,
            AppMenuDelegate appMenuDelegate,
            View decorView,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            View hardwareButtonAnchorView,
            Supplier<Rect> appRect,
            WindowAndroid windowAndroid) {
        mContext = context;
        mAppMenuDelegate = appMenuDelegate;
        mDelegate = delegate;
        mDecorView = decorView;
        mBlockers = new ArrayList<>();
        mObservers = new ArrayList<>();
        mHardwareButtonMenuAnchor = hardwareButtonAnchorView;
        mAppRect = appRect;
        mWindowAndroid = windowAndroid;

        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        mActivityLifecycleDispatcher.register(this);

        assert mHardwareButtonMenuAnchor != null
                : "Using AppMenu requires to have menu_anchor_stub view";
        mListObserver =
                new ListObserver<Void>() {
                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        assert mModelList != null && mAppMenu != null;
                        updateModelForHighlightAndClick(
                                mModelList,
                                mHighlightMenuId,
                                mAppMenu,
                                /* startIndex= */ index,
                                /* withAssertions= */ false);
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        assert mModelList != null;
                        updateModelForHighlightAndClick(
                                mModelList,
                                mHighlightMenuId,
                                mAppMenu,
                                /* startIndex= */ index,
                                /* withAssertions= */ false);
                    }
                };
    }

    /** Called when the containing activity is being destroyed. */
    void destroy() {
        // Prevent the menu window from leaking.
        mWindowAndroid
                .getKeyboardDelegate()
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        hideAppMenu();

        mActivityLifecycleDispatcher.unregister(this);
    }

    @Override
    public void menuItemContentChanged(int menuRowId) {
        if (mAppMenu != null) mAppMenu.menuItemContentChanged(menuRowId);
    }

    @Override
    public void clearMenuHighlight() {
        setMenuHighlight(null);
    }

    @Override
    public void setMenuHighlight(Integer highlightItemId) {
        boolean highlighting = highlightItemId != null;
        setMenuHighlight(highlightItemId, highlighting);
    }

    @Override
    public void setMenuHighlight(Integer highlightItemId, boolean shouldHighlightMenuButton) {
        if (mHighlightMenuId == null && highlightItemId == null) return;
        if (mHighlightMenuId != null && mHighlightMenuId.equals(highlightItemId)) return;
        mHighlightMenuId = highlightItemId;
        for (AppMenuObserver observer : mObservers) {
            observer.onMenuHighlightChanged(shouldHighlightMenuButton);
        }
    }

    /**
     * Show the app menu.
     *
     * @param anchorView Anchor view (usually a menu button) to be used for the popup, if null is
     *     passed then hardware menu button anchor will be used.
     * @param startDragging Whether dragging is started. For example, if the app menu is showed by
     *     tapping on a button, this should be false. If it is showed by start dragging down on the
     *     menu button, this should be true. Note that if anchorView is null, this must be false
     *     since we no longer support hardware menu button dragging.
     * @return True, if the menu is shown, false, if menu is not shown, example reasons: the menu is
     *     not yet available to be shown, or the menu is already showing.
     */
    // TODO(crbug.com/40479664): Fix this properly.
    @SuppressLint("ResourceType")
    boolean showAppMenu(View anchorView, boolean startDragging) {
        if (!shouldShowAppMenu() || isAppMenuShowing()) return false;

        TextBubble.dismissBubbles();
        boolean isByPermanentButton = false;

        Display display = DisplayAndroidManager.getDefaultDisplayForContext(mContext);
        int rotation = display.getRotation();
        if (anchorView == null) {
            // This fixes the bug where the bottom of the menu starts at the top of
            // the keyboard, instead of overlapping the keyboard as it should.
            int displayHeight = mContext.getResources().getDisplayMetrics().heightPixels;
            Rect rect = new Rect();
            mDecorView.getWindowVisibleDisplayFrame(rect);
            int statusBarHeight = rect.top;
            mHardwareButtonMenuAnchor.setY((displayHeight - statusBarHeight));

            anchorView = mHardwareButtonMenuAnchor;
            isByPermanentButton = true;
        }

        // If the anchor view used to show the popup or the activity's decor view is not attached
        // to window, we don't show the app menu because the window manager might have revoked
        // the window token for this activity. See https://crbug.com/1105831.
        if (!mDecorView.isAttachedToWindow()
                || !anchorView.isAttachedToWindow()
                || !anchorView.getRootView().isAttachedToWindow()) {
            return false;
        }

        assert !(isByPermanentButton && startDragging);

        List<CustomViewBinder> customViewBinders = mDelegate.getCustomViewBinders();
        Map<CustomViewBinder, Integer> customViewTypeOffsetMap =
                populateCustomViewBinderOffsetMap(customViewBinders, AppMenuItemType.NUM_ENTRIES);
        mModelList =
                mDelegate.getMenuItems(
                        (id) -> {
                            return getCustomItemViewType(
                                    id, customViewBinders, customViewTypeOffsetMap);
                        },
                        this);
        mModelList.addObserver(mListObserver);
        ContextThemeWrapper wrapper =
                new ContextThemeWrapper(mContext, R.style.OverflowMenuThemeOverlay);

        if (mAppMenu == null) {
            TypedArray a =
                    wrapper.obtainStyledAttributes(
                            new int[] {android.R.attr.listPreferredItemHeightSmall});
            int itemRowHeight = a.getDimensionPixelSize(0, 0);
            a.recycle();
            mAppMenu = new AppMenu(itemRowHeight, this, mContext.getResources());
            mAppMenuDragHelper = new AppMenuDragHelper(mContext, mAppMenu, itemRowHeight);
        }
        setupModelForHighlightAndClick(mModelList, mHighlightMenuId, mAppMenu);
        ModelListAdapter adapter = new ModelListAdapter(mModelList);
        mAppMenu.updateMenu(mModelList, adapter);
        registerViewBinders(
                customViewBinders,
                customViewTypeOffsetMap,
                adapter,
                mDelegate.shouldShowIconBeforeItem());

        Point pt = new Point();
        display.getSize(pt);

        KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                mWindowAndroid.getKeyboardDelegate();

        // If keyboard is showing, wait until keyboard disappears to set appRect
        if (keyboardVisibilityDelegate.isKeyboardShowing(mContext, anchorView)) {
            View finalAnchorView = anchorView;
            boolean finalIsByPermanentButton = isByPermanentButton;
            mKeyboardVisibilityListener =
                    isShowing -> {
                        if (!isShowing) {
                            setDisplayAndShowAppMenu(
                                    wrapper,
                                    finalAnchorView,
                                    finalIsByPermanentButton,
                                    rotation,
                                    mAppRect.get(),
                                    customViewBinders,
                                    startDragging);
                            keyboardVisibilityDelegate
                                    .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
                        }
                    };
            keyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
            keyboardVisibilityDelegate.hideKeyboard(anchorView);
        } else {
            setDisplayAndShowAppMenu(
                    wrapper,
                    anchorView,
                    isByPermanentButton,
                    rotation,
                    mAppRect.get(),
                    customViewBinders,
                    startDragging);
        }
        return true;
    }

    void appMenuDismissed() {
        mAppMenuDragHelper.finishDragging();
        mDelegate.onMenuDismissed();
    }

    @Override
    public boolean isAppMenuShowing() {
        return mAppMenu != null && mAppMenu.isShowing();
    }

    /**
     * @return The App Menu that the menu handler is interacting with.
     */
    public AppMenu getAppMenu() {
        return mAppMenu;
    }

    AppMenuDragHelper getAppMenuDragHelper() {
        return mAppMenuDragHelper;
    }

    @Override
    public void hideAppMenu() {
        if (mAppMenu != null && mAppMenu.isShowing()) {
            mAppMenu.dismiss();
            if (mModelList != null) {
                mModelList.removeObserver(mListObserver);
            }
        }
    }

    @Override
    public AppMenuButtonHelper createAppMenuButtonHelper() {
        return new AppMenuButtonHelperImpl(this);
    }

    @Override
    public void invalidateAppMenu() {
        if (mAppMenu != null) mAppMenu.invalidate();
    }

    @Override
    public void addObserver(AppMenuObserver observer) {
        mObservers.add(observer);
    }

    @Override
    public void removeObserver(AppMenuObserver observer) {
        mObservers.remove(observer);
    }

    // StartStopWithNativeObserver implementation
    @Override
    public void onStartWithNative() {}

    @Override
    public void onStopWithNative() {
        hideAppMenu();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        hideAppMenu();
    }

    @VisibleForTesting
    void onOptionsItemSelected(int itemId) {
        if (mTestOptionsItemSelectedListener != null) {
            mTestOptionsItemSelectedListener.onResult(itemId);
            return;
        }

        mAppMenuDelegate.onOptionsItemSelected(itemId, mDelegate.getBundleForMenuItem(itemId));
    }

    /**
     * Called by AppMenu to report that the App Menu visibility has changed.
     * @param isVisible Whether the App Menu is showing.
     */
    void onMenuVisibilityChanged(boolean isVisible) {
        for (int i = 0; i < mObservers.size(); ++i) {
            mObservers.get(i).onMenuVisibilityChanged(isVisible);
        }
    }

    /**
     * A notification that the header view has been inflated.
     * @param view The inflated view.
     */
    void onHeaderViewInflated(View view) {
        if (mDelegate != null) mDelegate.onHeaderViewInflated(this, view);
    }

    /**
     * A notification that the footer view has been inflated.
     * @param view The inflated view.
     */
    void onFooterViewInflated(View view) {
        if (mDelegate != null) mDelegate.onFooterViewInflated(this, view);
    }

    /**
     * Registers an {@link AppMenuBlocker} used to help determine whether the app menu can be shown.
     * @param blocker An {@link AppMenuBlocker} to check before attempting to show the app menu.
     */
    void registerAppMenuBlocker(AppMenuBlocker blocker) {
        if (!mBlockers.contains(blocker)) mBlockers.add(blocker);
    }

    /**
     * @param blocker The {@link AppMenuBlocker} to unregister.
     */
    void unregisterAppMenuBlocker(AppMenuBlocker blocker) {
        mBlockers.remove(blocker);
    }

    boolean shouldShowAppMenu() {
        for (int i = 0; i < mBlockers.size(); i++) {
            if (!mBlockers.get(i).canShowAppMenu()) return false;
        }
        return true;
    }

    void overrideOnOptionItemSelectedListenerForTests(
            Callback<Integer> onOptionsItemSelectedListener) {
        mTestOptionsItemSelectedListener = onOptionsItemSelectedListener;
    }

    AppMenuPropertiesDelegate getDelegateForTests() {
        return mDelegate;
    }

    private void registerViewBinders(
            @Nullable List<CustomViewBinder> customViewBinders,
            Map<CustomViewBinder, Integer> customViewTypeOffsetMap,
            ModelListAdapter adapter,
            boolean iconBeforeItem) {
        int standardItemResId = R.layout.menu_item;
        if (iconBeforeItem) {
            standardItemResId = R.layout.menu_item_start_with_icon;
        }

        adapter.registerType(
                AppMenuItemType.STANDARD,
                new LayoutViewBuilder(standardItemResId),
                AppMenuItemViewBinder::bindStandardItem);
        adapter.registerType(
                AppMenuItemType.TITLE_BUTTON,
                new LayoutViewBuilder(R.layout.title_button_menu_item),
                AppMenuItemViewBinder::bindTitleButtonItem);
        adapter.registerType(
                AppMenuItemType.THREE_BUTTON_ROW,
                new LayoutViewBuilder(R.layout.icon_row_menu_item),
                AppMenuItemViewBinder::bindIconRowItem);
        adapter.registerType(
                AppMenuItemType.FOUR_BUTTON_ROW,
                new LayoutViewBuilder(R.layout.icon_row_menu_item),
                AppMenuItemViewBinder::bindIconRowItem);
        adapter.registerType(
                AppMenuItemType.FIVE_BUTTON_ROW,
                new LayoutViewBuilder(R.layout.icon_row_menu_item),
                AppMenuItemViewBinder::bindIconRowItem);

        if (customViewBinders == null) return;
        for (int i = 0; i < customViewBinders.size(); i++) {
            CustomViewBinder binder = customViewBinders.get(i);
            if (customViewTypeOffsetMap.get(binder) == null) {
                continue;
            }

            for (int j = 0; j < binder.getViewTypeCount(); j++) {
                adapter.registerType(
                        customViewTypeOffsetMap.get(binder) + j,
                        new LayoutViewBuilder(binder.getLayoutId(j)),
                        binder);
            }
        }
    }

    void setupModelForHighlightAndClick(
            ModelList modelList, Integer highlightedId, AppMenuClickHandler appMenuClickHandler) {
        updateModelForHighlightAndClick(
                modelList,
                highlightedId,
                appMenuClickHandler,
                /* startIndex= */ 0,
                /* withAssertions= */ true);
    }

    private void updateModelForHighlightAndClick(
            ModelList modelList,
            Integer highlightedId,
            AppMenuClickHandler appMenuClickHandler,
            int startIndex,
            boolean withAssertions) {
        if (modelList == null) {
            return;
        }

        for (int i = startIndex; i < modelList.size(); i++) {
            PropertyModel model = modelList.get(i).model;
            if (withAssertions) {
                // Not like other keys which is set by AppMenuPropertiesDelegateImpl, CLICK_HANDLER
                // and HIGHLIGHTED should not be set yet.
                assert model.get(AppMenuItemProperties.CLICK_HANDLER) == null;
                assert !model.get(AppMenuItemProperties.HIGHLIGHTED);
            }
            model.set(AppMenuItemProperties.CLICK_HANDLER, appMenuClickHandler);
            model.set(AppMenuItemProperties.POSITION, i);

            if (highlightedId != null) {
                model.set(
                        AppMenuItemProperties.HIGHLIGHTED,
                        model.get(AppMenuItemProperties.MENU_ITEM_ID) == highlightedId);
                if (model.get(AppMenuItemProperties.SUBMENU) != null) {
                    ModelList subList = model.get(AppMenuItemProperties.SUBMENU);
                    for (int j = 0; j < subList.size(); j++) {
                        PropertyModel subModel = subList.get(j).model;
                        subModel.set(AppMenuItemProperties.CLICK_HANDLER, appMenuClickHandler);
                        subModel.set(
                                AppMenuItemProperties.HIGHLIGHTED,
                                subModel.get(AppMenuItemProperties.MENU_ITEM_ID) == highlightedId);
                    }
                }
            }
        }
    }

    private Map<CustomViewBinder, Integer> populateCustomViewBinderOffsetMap(
            @Nullable List<CustomViewBinder> customViewBinders, int startingOffset) {
        Map<CustomViewBinder, Integer> customViewTypeOffsetMap = new HashMap<>();
        if (customViewBinders == null) return customViewTypeOffsetMap;

        int currentOffset = startingOffset;
        for (int i = 0; i < customViewBinders.size(); i++) {
            CustomViewBinder binder = customViewBinders.get(i);
            customViewTypeOffsetMap.put(binder, currentOffset);
            currentOffset += binder.getViewTypeCount();
        }
        return customViewTypeOffsetMap;
    }

    private int getCustomItemViewType(
            int id,
            List<CustomViewBinder> customViewBinders,
            Map<CustomViewBinder, Integer> customViewTypeOffsetMap) {
        if (customViewBinders == null || customViewTypeOffsetMap == null) {
            return CustomViewBinder.NOT_HANDLED;
        }

        for (int i = 0; i < customViewBinders.size(); i++) {
            CustomViewBinder binder = customViewBinders.get(i);
            int binderViewType = binder.getItemViewType(id);
            if (binderViewType != CustomViewBinder.NOT_HANDLED) {
                return binderViewType + customViewTypeOffsetMap.get(binder);
            }
        }
        return CustomViewBinder.NOT_HANDLED;
    }

    /** @param reporter A means of reporting an exception without crashing. */
    static void setExceptionReporter(Callback<Throwable> reporter) {
        AppMenu.setExceptionReporter(reporter);
    }

    @Nullable
    ModelList getModelListForTesting() {
        return mModelList;
    }

    public View getKeyboardDelegate() {
        return mDecorView;
    }

    private void setDisplayAndShowAppMenu(
            ContextThemeWrapper wrapper,
            View anchorView,
            boolean isByPermanentButton,
            Integer rotation,
            Rect appRect,
            List<CustomViewBinder> customViewBinders,
            boolean startDragging) {
        // Use full size of window for abnormal appRect.
        if (appRect.left < 0 && appRect.top < 0) {
            appRect.left = 0;
            appRect.top = 0;
            appRect.right = mDecorView.getWidth();
            appRect.bottom = mDecorView.getHeight();
        }

        int footerResourceId = 0;
        if (mDelegate.shouldShowFooter(appRect.height())) {
            footerResourceId = mDelegate.getFooterResourceId();
        }
        int headerResourceId = 0;
        if (mDelegate.shouldShowHeader(appRect.height())) {
            headerResourceId = mDelegate.getHeaderResourceId();
        }
        mAppMenu.show(
                wrapper,
                anchorView,
                isByPermanentButton,
                rotation,
                appRect,
                footerResourceId,
                headerResourceId,
                mDelegate.getGroupDividerId(),
                mHighlightMenuId,
                customViewBinders,
                mDelegate.isMenuIconAtStart());
        mAppMenuDragHelper.onShow(startDragging);
        clearMenuHighlight();
        RecordUserAction.record("MobileMenuShow");
        mDelegate.onMenuShown();
    }
}
