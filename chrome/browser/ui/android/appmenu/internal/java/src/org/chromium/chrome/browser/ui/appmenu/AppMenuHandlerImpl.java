// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.SparseArray;
import android.view.ContextThemeWrapper;
import android.view.Display;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.LayoutRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
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
import java.util.List;
import java.util.function.Function;

/**
 * Object responsible for handling the creation, showing, hiding of the AppMenu and notifying the
 * AppMenuObservers about these actions.
 */
@NullMarked
class AppMenuHandlerImpl
        implements AppMenuHandler, StartStopWithNativeObserver, ConfigurationChangedObserver {
    private @Nullable AppMenu mAppMenu;
    private @Nullable AppMenuDragHelper mAppMenuDragHelper;
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
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private @Nullable ModelList mModelList;
    private final ListObserver<Void> mListObserver;
    private @Nullable Callback<Integer> mTestOptionsItemSelectedListener;
    private @MonotonicNonNull KeyboardVisibilityDelegate.KeyboardVisibilityListener
            mKeyboardVisibilityListener;

    /**
     * The resource id of the menu item to highlight when the menu next opens. A value of {@code
     * null} means no item will be highlighted. This value will be cleared after the menu is opened.
     */
    private @Nullable Integer mHighlightMenuId;

    /**
     * Constructs an AppMenuHandlerImpl object.
     *
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
     * @param browserControlsStateProvider a provider that can provide the state of the toolbar
     */
    public AppMenuHandlerImpl(
            Context context,
            AppMenuPropertiesDelegate delegate,
            AppMenuDelegate appMenuDelegate,
            View decorView,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            View hardwareButtonAnchorView,
            Supplier<Rect> appRect,
            WindowAndroid windowAndroid,
            BrowserControlsStateProvider browserControlsStateProvider) {
        mContext = context;
        mAppMenuDelegate = appMenuDelegate;
        mDelegate = delegate;
        mDecorView = decorView;
        mBlockers = new ArrayList<>();
        mObservers = new ArrayList<>();
        mHardwareButtonMenuAnchor = hardwareButtonAnchorView;
        mAppRect = appRect;
        mWindowAndroid = windowAndroid;
        mBrowserControlsStateProvider = browserControlsStateProvider;

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
                        assert mModelList != null && mAppMenu != null;
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
        if (mKeyboardVisibilityListener != null) {
            mWindowAndroid
                    .getKeyboardDelegate()
                    .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        }
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
    public void setMenuHighlight(@Nullable Integer highlightItemId) {
        boolean highlighting = highlightItemId != null;
        setMenuHighlight(highlightItemId, highlighting);
    }

    @Override
    public void setMenuHighlight(
            @Nullable Integer highlightItemId, boolean shouldHighlightMenuButton) {
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
    boolean showAppMenu(@Nullable View anchorView, boolean startDragging) {
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

        mModelList = mDelegate.getMenuItems(this);
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

        SparseArray<Function<Context, Integer>> customSizingProviders = new SparseArray<>();
        registerViewBinders(adapter, customSizingProviders, mDelegate.shouldShowIconBeforeItem());

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
                            assert mAppMenu != null;
                            setDisplayAndShowAppMenu(
                                    wrapper,
                                    finalAnchorView,
                                    finalIsByPermanentButton,
                                    rotation,
                                    mAppRect.get(),
                                    customSizingProviders,
                                    startDragging);
                            // https://github.com/uber/NullAway/issues/1190
                            assumeNonNull(mKeyboardVisibilityListener);
                            keyboardVisibilityDelegate.removeKeyboardVisibilityListener(
                                    mKeyboardVisibilityListener);
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
                    customSizingProviders,
                    startDragging);
        }
        return true;
    }

    void appMenuDismissed() {
        assumeNonNull(mAppMenuDragHelper);
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
    public @Nullable AppMenu getAppMenu() {
        return mAppMenu;
    }

    @Nullable AppMenuDragHelper getAppMenuDragHelper() {
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

    /**
     * Called when a menu item is selected.
     *
     * @param itemId The menu item ID.
     * @param triggeringMotion The {@link MotionEventInfo} that triggered the click; it is {@code
     *     null} if {@link MotionEvent} wasn't available when the click was detected, such as in
     *     {@link android.view.View.OnClickListener}.
     */
    @VisibleForTesting
    void onOptionsItemSelected(int itemId, @Nullable MotionEventInfo triggeringMotion) {
        if (mTestOptionsItemSelectedListener != null) {
            mTestOptionsItemSelectedListener.onResult(itemId);
            return;
        }

        mAppMenuDelegate.onOptionsItemSelected(
                itemId, mDelegate.getBundleForMenuItem(itemId), triggeringMotion);
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

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void registerDefaultViewBinders(
            ModelListAdapter adapter, boolean iconBeforeItem) {
        @LayoutRes
        int standardItemResId =
                iconBeforeItem ? R.layout.menu_item_start_with_icon : R.layout.menu_item;

        adapter.registerType(
                AppMenuItemType.STANDARD,
                new LayoutViewBuilder(standardItemResId),
                AppMenuItemViewBinder::bindStandardItem);
        adapter.registerType(
                AppMenuItemType.TITLE_BUTTON,
                new LayoutViewBuilder<ViewGroup>(R.layout.title_button_menu_item) {
                    @Override
                    protected ViewGroup postInflationInit(ViewGroup view) {
                        ViewStub stub = view.findViewById(R.id.menu_item_container_stub);
                        stub.setLayoutResource(standardItemResId);
                        View inflatedView = stub.inflate();
                        inflatedView.setDuplicateParentStateEnabled(true);
                        return view;
                    }
                },
                AppMenuItemViewBinder::bindTitleButtonItem);
        adapter.registerType(
                AppMenuItemType.BUTTON_ROW,
                new LayoutViewBuilder(R.layout.icon_row_menu_item),
                AppMenuItemViewBinder::bindIconRowItem);
        adapter.registerType(
                AppMenuItemType.DIVIDER,
                new LayoutViewBuilder(R.layout.divider_line_menu_item),
                DividerLineMenuItemViewBinder::bind);
    }

    private void registerViewBinders(
            ModelListAdapter adapter,
            SparseArray<Function<Context, Integer>> customSizingProviders,
            boolean iconBeforeItem) {
        registerDefaultViewBinders(adapter, iconBeforeItem);
        customSizingProviders.append(
                AppMenuItemType.DIVIDER, DividerLineMenuItemViewBinder::getPixelHeight);

        mDelegate.registerCustomViewBinders(adapter, customSizingProviders);
    }

    void setupModelForHighlightAndClick(
            ModelList modelList,
            @Nullable Integer highlightedId,
            AppMenuClickHandler appMenuClickHandler) {
        updateModelForHighlightAndClick(
                modelList,
                highlightedId,
                appMenuClickHandler,
                /* startIndex= */ 0,
                /* withAssertions= */ true);
    }

    private void updateModelForHighlightAndClick(
            ModelList modelList,
            @Nullable Integer highlightedId,
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
                if (model.get(AppMenuItemProperties.ADDITIONAL_ICONS) != null) {
                    ModelList subList = model.get(AppMenuItemProperties.ADDITIONAL_ICONS);
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

    /**
     * @param reporter A means of reporting an exception without crashing.
     */
    static void setExceptionReporter(Callback<Throwable> reporter) {
        AppMenu.setExceptionReporter(reporter);
    }

    @Nullable ModelList getModelListForTesting() {
        return mModelList;
    }

    public View getKeyboardDelegate() {
        return mDecorView;
    }

    @RequiresNonNull("mAppMenu")
    private void setDisplayAndShowAppMenu(
            ContextThemeWrapper wrapper,
            View anchorView,
            boolean isByPermanentButton,
            Integer rotation,
            Rect appRect,
            SparseArray<Function<Context, Integer>> customSizingProviders,
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
                customSizingProviders,
                mDelegate.isMenuIconAtStart(),
                mBrowserControlsStateProvider.getControlsPosition(),
                addTopPaddingBeforeFirstRow());
        assumeNonNull(mAppMenuDragHelper);
        mAppMenuDragHelper.onShow(startDragging);
        clearMenuHighlight();
        RecordUserAction.record("MobileMenuShow");
        mDelegate.onMenuShown();
    }

    private boolean addTopPaddingBeforeFirstRow() {
        if (mModelList == null || mModelList.isEmpty()) return false;
        return mModelList.get(0).type != AppMenuItemType.BUTTON_ROW;
    }
}
