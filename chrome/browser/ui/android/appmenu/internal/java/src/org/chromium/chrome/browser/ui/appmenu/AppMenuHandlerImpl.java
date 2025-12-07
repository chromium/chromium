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
import android.text.TextUtils;
import android.util.SparseArray;
import android.view.ContextThemeWrapper;
import android.view.Display;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.Adapter;

import androidx.annotation.ColorInt;
import androidx.annotation.LayoutRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenu.AppMenuPopup;
import org.chromium.chrome.browser.ui.appmenu.internal.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroidManager;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.SubmenuHeaderFactory;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Function;
import java.util.function.Supplier;

/**
 * Object responsible for handling the creation, showing, hiding of the AppMenu and notifying the
 * AppMenuObservers about these actions.
 */
@NullMarked
class AppMenuHandlerImpl
        implements AppMenuHandler,
                AppMenuClickHandler,
                AppMenu.AppMenuVisibilityDelegate,
                StartStopWithNativeObserver,
                ConfigurationChangedObserver,
                FlyoutHandler<AppMenuPopup> {

    /** An {@link Adapter} for the list of items in the app menu. */
    private static final class AppMenuAdapter extends ModelListAdapter {
        AppMenuAdapter(ModelList modelList) {
            super(modelList);
        }

        @Override
        public boolean areAllItemsEnabled() {
            for (int i = 0; i < getCount(); i++) {
                if (!isEnabled(i)) {
                    return false;
                }
            }
            return true;
        }

        @Override
        public boolean isEnabled(int position) {
            return getItemViewType(position) != AppMenuItemType.DIVIDER
                    && ((ListItem) getItem(position)).model.get(AppMenuItemProperties.ENABLED);
        }
    }

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
    private @Nullable HierarchicalMenuController mHierarchicalMenuController;
    private final SubmenuHeaderFactory mSubmenuHeaderFactory;
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
     * @param submenuHeaderFactory The {@link SubmenuHeaderFactory} to use for the {@link
     *     HierarchicalMenuController}.
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
            BrowserControlsStateProvider browserControlsStateProvider,
            SubmenuHeaderFactory submenuHeaderFactory) {
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
        mSubmenuHeaderFactory = submenuHeaderFactory;

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
                                AppMenuHandlerImpl.this,
                                /* startIndex= */ index,
                                /* withAssertions= */ false);
                        mAppMenu.updateMenuHeight();
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        assert mModelList != null && mAppMenu != null;
                        updateModelForHighlightAndClick(
                                mModelList,
                                mHighlightMenuId,
                                AppMenuHandlerImpl.this,
                                /* startIndex= */ index,
                                /* withAssertions= */ false);
                        mAppMenu.updateMenuHeight();
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

    @Override
    public void setContentDescription(@Nullable String desc) {
        if (mAppMenu != null) mAppMenu.setContentDescription(desc);
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

        mModelList = mDelegate.getMenuItems();
        mModelList.addObserver(mListObserver);
        ContextThemeWrapper wrapper =
                new ContextThemeWrapper(mContext, R.style.AppMenuThemeOverlay);

        TypedArray a = wrapper.obtainStyledAttributes(new int[] {R.attr.listItemHeight});
        int itemRowHeight = a.getDimensionPixelSize(0, 0);
        a.recycle();

        if (mHierarchicalMenuController == null) {
            mHierarchicalMenuController =
                    new HierarchicalMenuController(
                            mContext, new AppMenuUtil.AppMenuKeyProvider(), mSubmenuHeaderFactory);
        }

        mHierarchicalMenuController.setupCallbacksRecursively(
                /* headerModelList= */ null, mModelList, () -> {});

        if (mAppMenu == null) {
            mAppMenu =
                    new AppMenu(
                            this,
                            mContext.getResources(),
                            mHierarchicalMenuController,
                            mAppMenuDelegate.shouldDisableVerticalScrollbar());
            mAppMenuDragHelper = new AppMenuDragHelper(mContext, mAppMenu, itemRowHeight);
        }

        setupModelForHighlightAndClick(mModelList, mHighlightMenuId, this);

        AppMenuAdapter adapter = new AppMenuAdapter(mModelList);
        SparseArray<Function<Context, Integer>> customSizingProviders = new SparseArray<>();
        registerViewBinders(adapter, customSizingProviders, mDelegate.shouldShowIconBeforeItem());

        AppMenu.InitialSizingHelper initialSizingHelper =
                new AppMenu.InitialSizingHelper() {
                    @Override
                    public int getInitialHeightForView(int index) {
                        if (mModelList == null) {
                            assert false : "ModelList is null";
                            return 0;
                        }
                        Function<Context, Integer> customSizingProvider =
                                customSizingProviders.get(mModelList.get(index).type);
                        if (customSizingProvider != null) {
                            return customSizingProvider.apply(mContext);
                        }
                        return itemRowHeight;
                    }

                    @Override
                    public boolean canBeLastVisibleInitialView(int index) {
                        if (mModelList == null) {
                            assert false : "ModelList is null";
                            return false;
                        }
                        return mModelList.get(index).type != AppMenuItemType.DIVIDER;
                    }
                };

        mAppMenu.updateMenu(initialSizingHelper, adapter);

        Point pt = new Point();
        display.getSize(pt);

        KeyboardVisibilityDelegate keyboardVisibilityDelegate =
                mWindowAndroid.getKeyboardDelegate();

        // If keyboard is showing, wait until keyboard disappears to set appRect
        if (keyboardVisibilityDelegate.isKeyboardShowing(anchorView)) {
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
                    startDragging);
        }
        return true;
    }

    @Override
    public void appMenuDismissed() {
        assumeNonNull(mAppMenuDragHelper);
        mAppMenuDragHelper.finishDragging();
        mDelegate.onMenuDismissed();
        if (mModelList != null) {
            mModelList.removeObserver(mListObserver);
        }
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
        }
    }

    @Override
    public AppMenuButtonHelper createAppMenuButtonHelper() {
        return new AppMenuButtonHelperImpl(this);
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

    @Override
    public void onItemClick(PropertyModel model, @Nullable MotionEventInfo triggeringMotion) {
        if (mAppMenu == null) return;
        if (!model.get(AppMenuItemProperties.ENABLED)) return;

        int itemId = model.get(AppMenuItemProperties.MENU_ITEM_ID);
        mAppMenu.setSelectedItemBeforeDismiss(true);
        mAppMenu.dismiss();

        if (mTestOptionsItemSelectedListener != null) {
            mTestOptionsItemSelectedListener.onResult(itemId);
            return;
        }

        mAppMenuDelegate.onOptionsItemSelected(
                itemId, mDelegate.getBundleForMenuItem(itemId), triggeringMotion);
    }

    @Override
    public boolean onItemLongClick(PropertyModel model, View view) {
        if (mAppMenu == null) return false;
        if (!model.get(AppMenuItemProperties.ENABLED)) return false;

        mAppMenu.setSelectedItemBeforeDismiss(true);

        CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
        CharSequence message =
                TextUtils.isEmpty(titleCondensed)
                        ? model.get(AppMenuItemProperties.TITLE)
                        : titleCondensed;
        return showToastForItem(message, view);
    }

    private static boolean showToastForItem(CharSequence message, View view) {
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
    public void onMenuVisibilityChanged(boolean isVisible) {
        for (int i = 0; i < mObservers.size(); ++i) {
            mObservers.get(i).onMenuVisibilityChanged(isVisible);
        }
    }

    /**
     * Registers an {@link AppMenuBlocker} used to help determine whether the app menu can be shown.
     *
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

    @Override
    public AppMenuPropertiesDelegate getMenuPropertiesDelegate() {
        return mDelegate;
    }

    @VisibleForTesting
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
                AppMenuItemType.MENU_ITEM_WITH_SUBMENU,
                new LayoutViewBuilder(R.layout.menu_item_with_submenu),
                AppMenuItemViewBinder::bindItemWithSubmenu);
        adapter.registerType(
                AppMenuItemType.SUBMENU_HEADER,
                new LayoutViewBuilder(R.layout.submenu_header),
                AppMenuItemViewBinder::bindSubmenuHeader);
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

        updateModelForHighlightAndClickRecursively(
                modelList::get,
                modelList::size,
                highlightedId,
                appMenuClickHandler,
                startIndex,
                withAssertions);
    }

    /**
     * Recursively sets the click handler, position, and highlight state for all items in a
     * list-like structure and its nested sub-lists. This helper is abstracted to work on any
     * list-like structure (e.g., {@link ModelList} or {@link java.util.List}) by using function
     * references.
     *
     * @param itemGetter A function to retrieve a {@link ListItem} by its index (e.g., {@code
     *     list::get}).
     * @param sizeGetter A supplier for the list's total size (e.g., {@code list::size}).
     * @param highlightedId The menu item ID to mark as highlighted. If null, no item will be
     *     marked.
     * @param appMenuClickHandler The {@link AppMenuClickHandler} to attach to each menu item.
     * @param startIndex The index in the list to start processing from.
     * @param withAssertions Whether to run assertions (e.g., that handlers aren't already set).
     */
    private void updateModelForHighlightAndClickRecursively(
            Function<Integer, ListItem> itemGetter,
            Supplier<Integer> sizeGetter,
            @Nullable Integer highlightedId,
            AppMenuClickHandler appMenuClickHandler,
            int startIndex,
            boolean withAssertions) {
        for (int i = startIndex; i < sizeGetter.get(); i++) {
            PropertyModel model = itemGetter.apply(i).model;

            if (withAssertions) {
                // Not like other keys which is set by AppMenuPropertiesDelegateImpl, CLICK_HANDLER
                // and HIGHLIGHTED should not be set yet.
                assert !model.containsKey(AppMenuItemProperties.CLICK_HANDLER)
                        || model.get(AppMenuItemProperties.CLICK_HANDLER) == null;
                assert !model.get(AppMenuItemProperties.HIGHLIGHTED);
            }

            if (model.containsKey(AppMenuItemProperties.CLICK_HANDLER)) {
                model.set(AppMenuItemProperties.CLICK_HANDLER, appMenuClickHandler);
            }

            if (model.containsKey(AppMenuItemProperties.POSITION)) {
                model.set(AppMenuItemProperties.POSITION, i);
            }

            if (highlightedId != null) {
                model.set(
                        AppMenuItemProperties.HIGHLIGHTED,
                        model.get(AppMenuItemProperties.MENU_ITEM_ID) == highlightedId);
            }

            if (model.containsKey(AppMenuItemProperties.ADDITIONAL_ICONS)) {
                ModelList additionalIcons = model.get(AppMenuItemProperties.ADDITIONAL_ICONS);
                if (additionalIcons != null) {
                    updateModelForHighlightAndClickRecursively(
                            additionalIcons::get,
                            additionalIcons::size,
                            highlightedId,
                            appMenuClickHandler,
                            /* startIndex= */ 0,
                            withAssertions);
                }
            }

            if (model.containsKey(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS)) {
                List<ListItem> submenuItems =
                        model.get(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS);
                if (submenuItems != null) {
                    updateModelForHighlightAndClickRecursively(
                            submenuItems::get,
                            submenuItems::size,
                            highlightedId,
                            appMenuClickHandler,
                            /* startIndex= */ 0,
                            withAssertions);
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
            boolean startDragging) {
        // Use full size of window for abnormal appRect.
        if (appRect.left < 0 && appRect.top < 0) {
            appRect.left = 0;
            appRect.top = 0;
            appRect.right = mDecorView.getWidth();
            appRect.bottom = mDecorView.getHeight();
        }

        mAppMenu.show(
                wrapper,
                anchorView,
                isByPermanentButton,
                rotation,
                appRect,
                mDelegate.buildFooterView(this),
                mDelegate.buildHeaderView(),
                mHighlightMenuId,
                mDelegate.isMenuIconAtStart(),
                mBrowserControlsStateProvider.getControlsPosition(),
                addTopPaddingBeforeFirstRow(),
                this);
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

    @Override
    public Rect getPopupRect(AppMenuPopup popupWindow) {
        return popupWindow.getPopupRect();
    }

    @Override
    public void dismissPopup(AppMenuPopup popupWindow) {
        popupWindow.dismiss();
    }

    @Override
    public void setWindowFocus(AppMenuPopup popupWindow, boolean hasFocus) {
        ViewGroup contentView = (ViewGroup) popupWindow.getContentView();
        if (contentView == null) return;

        HierarchicalMenuController.setWindowFocusForFlyoutMenus(contentView, hasFocus);
    }

    @Override
    public AppMenuPopup createAndShowFlyoutPopup(
            ListItem item, View view, Runnable dismissRunnable) {
        AppMenuAdapter adapter = new AppMenuAdapter(getModelListSubtree(item));
        SparseArray<Function<Context, Integer>> customSizingProviders = new SparseArray<>();
        registerViewBinders(adapter, customSizingProviders, mDelegate.shouldShowIconBeforeItem());

        assert mAppMenu != null;
        return mAppMenu.createAndShowFlyoutPopup(adapter, view, item, dismissRunnable);
    }

    private static ModelList getModelListSubtree(ListItem item) {
        ModelList modelList = new ModelList();
        for (ListItem listItem : item.model.get(AppMenuItemWithSubmenuProperties.SUBMENU_ITEMS)) {
            modelList.add(listItem);
        }
        return modelList;
    }
}
