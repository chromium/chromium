// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Nullable;
import android.support.annotation.Px;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Supplier;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.InsetObserverView;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Provider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashMap;
import java.util.Map;

/**
 * This part of the manual filling component manages the state of the manual filling flow depending
 * on the currently shown tab.
 */
class ManualFillingMediator extends EmptyTabObserver
        implements KeyboardAccessoryCoordinator.VisibilityDelegate, View.OnLayoutChangeListener {
    private WindowAndroid mWindowAndroid;
    private Supplier<InsetObserverView> mInsetObserverViewSupplier;
    private boolean mShouldShow = false;
    private final KeyboardExtensionSizeManager mKeyboardExtensionSizeManager =
            new KeyboardExtensionSizeManager();

    /**
     * Provides a cache for a given Provider which can repeat the last notification to all
     * observers.
     */
    private class ActionProviderCacheAdapter extends KeyboardAccessoryData.PropertyProvider<Action>
            implements KeyboardAccessoryData.Observer<Action> {
        private final Tab mTab;
        private Action[] mLastItems;

        /**
         * Creates an adapter that listens to the given |provider| and stores items provided by it.
         * If the observed provider serves a currently visible tab, the data is immediately sent on.
         * @param tab The {@link Tab} which the given Provider should affect immediately.
         * @param provider The {@link Provider} to observe and whose data to cache.
         * @param defaultItems The items to be notified about if the Provider hasn't provided any.
         */
        ActionProviderCacheAdapter(Tab tab, KeyboardAccessoryData.PropertyProvider<Action> provider,
                Action[] defaultItems) {
            super(provider.mType);
            mTab = tab;
            provider.addObserver(this);
            mLastItems = defaultItems;
        }

        /**
         * Calls {@link #onItemsAvailable} with the last used items again. If there haven't been
         * any calls, call it with an empty list to avoid putting observers in an undefined state.
         */
        void notifyAboutCachedItems() {
            notifyObservers(mLastItems);
        }

        @Override
        public void onItemsAvailable(int typeId, Action[] actions) {
            mLastItems = actions;
            // Update the contents immediately, if the adapter connects to an active element.
            if (mTab == mActiveBrowserTab) notifyObservers(actions);
        }
    }

    /**
     * This class holds all data that is necessary to restore the state of the Keyboard accessory
     * and its sheet for a given tab.
     */
    @VisibleForTesting
    static class AccessoryState {
        @Nullable
        ActionProviderCacheAdapter mActionsProvider;
        @Nullable
        PasswordAccessorySheetCoordinator mPasswordAccessorySheet;
    }

    // TODO(fhorschig): Do we need a MapObservable type? (This would be only observer though).
    private final Map<Tab, AccessoryState> mModel = new HashMap<>();
    private KeyboardAccessoryCoordinator mKeyboardAccessory;
    private AccessorySheetCoordinator mAccessorySheet;
    private ChromeActivity mActivity; // Used to control the keyboard.
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private Tab mActiveBrowserTab;
    private DropdownPopupWindow mPopup;

    private final SceneChangeObserver mTabSwitcherObserver = new SceneChangeObserver() {
        @Override
        public void onTabSelectionHinted(int tabId) {}

        @Override
        public void onSceneStartShowing(Layout layout) {
            // Includes events like side-swiping between tabs and triggering contextual search.
            pause();
        }

        @Override
        public void onSceneChange(Layout layout) {}
    };

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onHidden(Tab tab, @TabHidingType int type) {
            pause();
        }

        @Override
        public void onDestroyed(Tab tab) {
            mModel.remove(tab); // Clears tab if still present.
            if (tab == mActiveBrowserTab) mActiveBrowserTab = null;
            restoreCachedState(mActiveBrowserTab);
        }

        @Override
        public void onEnterFullscreenMode(Tab tab, FullscreenOptions options) {
            pause();
        }
    };

    void initialize(KeyboardAccessoryCoordinator keyboardAccessory,
            AccessorySheetCoordinator accessorySheet, WindowAndroid windowAndroid) {
        mActivity = (ChromeActivity) windowAndroid.getActivity().get();
        assert mActivity != null;
        mWindowAndroid = windowAndroid;
        mKeyboardAccessory = keyboardAccessory;
        mAccessorySheet = accessorySheet;
        setInsetObserverViewSupplier(mActivity::getInsetObserverView);
        LayoutManager manager = getLayoutManager();
        if (manager != null) manager.addSceneChangeObserver(mTabSwitcherObserver);
        mActivity.findViewById(android.R.id.content).addOnLayoutChangeListener(this);
        mTabModelObserver = new TabModelSelectorTabModelObserver(mActivity.getTabModelSelector()) {
            @Override
            public void didSelectTab(Tab tab, @TabModel.TabSelectionType int type, int lastId) {
                mActiveBrowserTab = tab;
                restoreCachedState(tab);
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                mModel.remove(tab);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                if (mActiveBrowserTab == tab) mActiveBrowserTab = null;
                restoreCachedState(mActiveBrowserTab);
            }
        };
        Tab currentTab = mActivity.getTabModelSelector().getCurrentTab();
        if (currentTab != null) {
            mTabModelObserver.didSelectTab(
                    currentTab, TabModel.TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        }
    }

    boolean isInitialized() {
        return mWindowAndroid != null;
    }

    boolean isFillingViewShown() {
        return mAccessorySheet != null && mAccessorySheet.isShown();
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (mActivity == null) return; // Activity has been cleaned up already.
        onKeyboardVisibilityPossiblyChanged(
                getKeyboardDelegate().isSoftKeyboardShowing(mActivity, view));
    }

    private void onKeyboardVisibilityPossiblyChanged(boolean isShowing) {
        if (!mKeyboardAccessory.hasContents()) return; // Exit early to not affect the layout.
        if (isShowing) {
            if (mShouldShow) {
                displayKeyboardAccessory();
            }
        } else {
            mKeyboardAccessory.close();
            onBottomControlSpaceChanged();
            if (mKeyboardAccessory.hasActiveTab()) {
                mAccessorySheet.show();
            }
        }
    }

    void registerPasswordProvider(Provider<KeyboardAccessoryData.Item> itemProvider) {
        PasswordAccessorySheetCoordinator accessorySheet = getPasswordAccessorySheet();
        if (accessorySheet == null) return; // Not available or initialized yet.
        accessorySheet.registerItemProvider(itemProvider);
    }

    void registerActionProvider(KeyboardAccessoryData.PropertyProvider<Action> actionProvider) {
        if (!isInitialized()) return;
        if (mActiveBrowserTab == null) return;
        ActionProviderCacheAdapter adapter =
                new ActionProviderCacheAdapter(mActiveBrowserTab, actionProvider, new Action[0]);
        mModel.get(mActiveBrowserTab).mActionsProvider = adapter;
        mKeyboardAccessory.registerActionListProvider(adapter);
    }

    void destroy() {
        if (!isInitialized()) return;
        pause();
        mActivity.findViewById(android.R.id.content).removeOnLayoutChangeListener(this);
        LayoutManager manager = getLayoutManager();
        if (manager != null) manager.removeSceneChangeObserver(mTabSwitcherObserver);
        mWindowAndroid = null;
        mActivity = null;
        mTabModelObserver.destroy();
    }

    boolean handleBackPress() {
        if (isInitialized() && mAccessorySheet.isShown()) {
            pause();
            return true;
        }
        return false;
    }

    void dismiss() {
        if (!isInitialized()) return;
        pause();
        ViewGroup contentView = getContentView();
        if (contentView != null) getKeyboard().hideSoftKeyboardOnly(contentView);
    }

    void notifyPopupOpened(DropdownPopupWindow popup) {
        mPopup = popup;
    }

    void showWhenKeyboardIsVisible() {
        ViewGroup contentView = getContentView();
        if (!isInitialized() || !mKeyboardAccessory.hasContents() || mShouldShow
                || contentView == null) {
            return;
        }
        mShouldShow = true;
        if (getKeyboard().isSoftKeyboardShowing(mActivity, contentView)) {
            displayKeyboardAccessory();
        }
    }

    void hide() {
        mShouldShow = false;
        pause();
    }

    void pause() {
        if (!isInitialized()) return;
        mKeyboardAccessory.dismiss();
    }

    void resume() {
        if (!isInitialized()) return;
        restoreCachedState(mActiveBrowserTab);
    }

    private void displayKeyboardAccessory() {
        // Don't open the accessory inside the contextual search panel.
        ContextualSearchManager contextualSearchManager = mActivity.getContextualSearchManager();
        if (contextualSearchManager != null && contextualSearchManager.isSearchPanelOpened()) {
            return;
        }
        mKeyboardAccessory.requestShowing();
        mKeyboardExtensionSizeManager.setKeyboardExtensionHeight(calculateAccessoryBarHeight());
        mKeyboardAccessory.closeActiveTab();
        mKeyboardAccessory.setBottomOffset(0);
        mAccessorySheet.hide();
    }

    KeyboardExtensionSizeManager getKeyboardExtensionSizeManager() {
        return mKeyboardExtensionSizeManager;
    }

    @Override
    public void onChangeAccessorySheet(int tabIndex) {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mAccessorySheet.setActiveTab(tabIndex);
        if (mPopup != null && mPopup.isShowing()) mPopup.dismiss();
        // If there is a keyboard, update the accessory sheet's height and hide the keyboard.
        ViewGroup contentView = getContentView();
        if (contentView == null) return; // Apparently the tab was cleaned up already.
        View rootView = contentView.getRootView();
        if (rootView == null) return;
        mAccessorySheet.setHeight(calculateAccessorySheetHeight(rootView));
        getKeyboard().hideSoftKeyboardOnly(contentView);
    }

    @Override
    public void onCloseAccessorySheet() {
        ViewGroup contentView = getContentView();
        if (contentView == null) return; // The tab was cleaned up already.
        if (getKeyboard().isSoftKeyboardShowing(mActivity, contentView)) {
            return; // If the keyboard is showing or is starting to show, the sheet closes gently.
        }
        mKeyboardExtensionSizeManager.setKeyboardExtensionHeight(0);
        mKeyboardAccessory.closeActiveTab();
        mKeyboardAccessory.setBottomOffset(0);
        mAccessorySheet.hide();
    }

    /**
     * Opens the keyboard which implicitly dismisses the sheet. Without open sheet, this is a NoOp.
     */
    void swapSheetWithKeyboard() {
        if (isInitialized() && mAccessorySheet.isShown()) onOpenKeyboard();
    }

    @Override
    public void onOpenKeyboard() {
        assert mActivity != null : "ManualFillingMediator needs initialization.";
        mKeyboardExtensionSizeManager.setKeyboardExtensionHeight(calculateAccessoryBarHeight());
        if (mActivity.getCurrentFocus() != null) {
            getKeyboard().showKeyboard(mActivity.getCurrentFocus());
        }
    }

    @Override
    public void onBottomControlSpaceChanged() {
        int newControlsHeight = calculateAccessoryBarHeight();
        int newControlsOffset = 0;
        if (mKeyboardAccessory.hasActiveTab()) {
            newControlsHeight += mAccessorySheet.getHeight();
            newControlsOffset += mAccessorySheet.getHeight();
        }
        mKeyboardAccessory.setBottomOffset(newControlsOffset);
        mKeyboardExtensionSizeManager.setKeyboardExtensionHeight(
                mKeyboardAccessory.isShown() ? newControlsHeight : 0);
        mActivity.getFullscreenManager().updateViewportSize();
    }

    /**
     * When trying to get the content of the active tab, there are several cases where a component
     * can be null - usually use before initialization or after destruction.
     * This helper ensures that the IDE warns about unchecked use of the all Nullable methods and
     * provides a shorthand for checking that all components are ready to use.
     * @return The content {@link View} of the held {@link ChromeActivity} or null if any part of it
     *         isn't ready to use.
     */
    private @Nullable ViewGroup getContentView() {
        if (mActivity == null) return null;
        Tab tab = mActivity.getActivityTab();
        if (tab == null) return null;
        return tab.getContentView();
    }

    /**
     * Shorthand to check whether there is a valid {@link LayoutManager} for the current activity.
     * If there isn't (e.g. before initialization or after destruction), return null.
     * @return {@code null} or a {@link LayoutManager}.
     */
    private @Nullable LayoutManager getLayoutManager() {
        if (mActivity == null) return null;
        CompositorViewHolder compositorViewHolder = mActivity.getCompositorViewHolder();
        if (compositorViewHolder == null) return null;
        return compositorViewHolder.getLayoutManager();
    }

    private ChromeKeyboardVisibilityDelegate getKeyboard() {
        KeyboardVisibilityDelegate delegate = mWindowAndroid.getKeyboardDelegate();
        return (ChromeKeyboardVisibilityDelegate) delegate;
    }

    private AccessoryState getOrCreateAccessoryState(Tab tab) {
        assert tab != null : "Accessory state was requested without providing a non-null tab!";
        AccessoryState state = mModel.get(tab);
        if (state != null) return state;
        state = new AccessoryState();
        mModel.put(tab, state);
        tab.addObserver(mTabObserver);
        return state;
    }

    private void restoreCachedState(Tab browserTab) {
        pause();
        clearTabs();
        if (browserTab == null) return; // If there is no tab, exit after cleaning everything.
        AccessoryState state = getOrCreateAccessoryState(browserTab);
        if (state.mPasswordAccessorySheet != null) {
            addTab(state.mPasswordAccessorySheet.getTab());
        }
        if (state.mActionsProvider != null) state.mActionsProvider.notifyAboutCachedItems();
    }

    private void clearTabs() {
        mKeyboardAccessory.setTabs(new KeyboardAccessoryData.Tab[0]);
        mAccessorySheet.setTabs(new KeyboardAccessoryData.Tab[0]);
    }

    private @Px int calculateAccessorySheetHeight(View rootView) {
        InsetObserverView insetObserver = mInsetObserverViewSupplier.get();
        if (insetObserver != null) return insetObserver.getSystemWindowInsetsBottom();
        // Without known inset (which is keyboard + bottom soft keys), use the keyboard height.
        return Math.max(mActivity.getResources().getDimensionPixelSize(
                                org.chromium.chrome.R.dimen.keyboard_accessory_suggestion_height),
                getKeyboard().calculateKeyboardHeight(rootView));
    }

    private @Px int calculateAccessoryBarHeight() {
        if (!mKeyboardAccessory.isShown()) return 0;
        return mActivity.getResources().getDimensionPixelSize(
                org.chromium.chrome.R.dimen.keyboard_accessory_suggestion_height);
    }

    private ChromeKeyboardVisibilityDelegate getKeyboardDelegate() {
        assert mWindowAndroid instanceof ChromeWindow;
        assert mWindowAndroid.getKeyboardDelegate() instanceof ChromeKeyboardVisibilityDelegate;
        return (ChromeKeyboardVisibilityDelegate) mWindowAndroid.getKeyboardDelegate();
    }

    @VisibleForTesting
    void addTab(KeyboardAccessoryData.Tab tab) {
        if (!isInitialized()) return;
        // TODO(fhorschig): This should add the tab only to the state. Sheet and accessory should be
        // using a |set| method or even observe the state.
        mKeyboardAccessory.addTab(tab);
        mAccessorySheet.addTab(tab);
    }

    @VisibleForTesting
    @Nullable
    PasswordAccessorySheetCoordinator getPasswordAccessorySheet() {
        if (!isInitialized()) return null;
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.EXPERIMENTAL_UI)
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORDS_KEYBOARD_ACCESSORY)) {
            return null;
        }
        if (mActiveBrowserTab == null) return null; // No need for a sheet if there is no tab.
        AccessoryState state = getOrCreateAccessoryState(mActiveBrowserTab);
        if (state.mPasswordAccessorySheet == null) {
            state.mPasswordAccessorySheet = new PasswordAccessorySheetCoordinator(mActivity);
            addTab(state.mPasswordAccessorySheet.getTab());
        }
        return state.mPasswordAccessorySheet;
    }

    @VisibleForTesting
    void setInsetObserverViewSupplier(Supplier<InsetObserverView> insetObserverViewSupplier) {
        mInsetObserverViewSupplier = insetObserverViewSupplier;
    }

    // TODO(fhorschig): Should be @VisibleForTesting.
    @Nullable
    KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mKeyboardAccessory;
    }

    @VisibleForTesting
    AccessorySheetCoordinator getAccessorySheet() {
        return mAccessorySheet;
    }

    @VisibleForTesting
    TabModelObserver getTabModelObserverForTesting() {
        return mTabModelObserver;
    }

    @VisibleForTesting
    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    @VisibleForTesting
    Map<Tab, AccessoryState> getModelForTesting() {
        return mModel;
    }
}
