// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KEYBOARD_EXTENSION_STATE;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.EXTENDING_KEYBOARD;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.FLOATING_BAR;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.FLOATING_SHEET;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.HIDDEN;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.REPLACING_KEYBOARD;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.WAITING_TO_REPLACE;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.PORTRAIT_ORIENTATION;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.SHOW_WHEN_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.SUPPRESSED_BY_BOTTOM_SHEET;

import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.StateProperty;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AddressAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.CreditCardAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.PasswordAccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.TouchToFillSheetCoordinator;
import org.chromium.chrome.browser.password_manager.ConfirmationDialogHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.HashSet;

/**
 * This part of the manual filling component manages the state of the manual filling flow depending
 * on the currently shown tab.
 */
class ManualFillingMediator extends EmptyTabObserver
        implements KeyboardAccessoryCoordinator.VisibilityDelegate, View.OnLayoutChangeListener {
    private static final int MINIMAL_AVAILABLE_VERTICAL_SPACE = 128; // in DP.
    private static final int MINIMAL_AVAILABLE_HORIZONTAL_SPACE = 180; // in DP.

    private PropertyModel mModel = ManualFillingProperties.createFillingModel();
    private WindowAndroid mWindowAndroid;
    private Supplier<InsetObserverView> mInsetObserverViewSupplier;
    private final ObservableSupplierImpl<Integer> mViewportInsetSupplier =
            new ObservableSupplierImpl<>();
    private final ManualFillingStateCache mStateCache = new ManualFillingStateCache();
    private final HashSet<Tab> mObservedTabs = new HashSet<>();
    private KeyboardAccessoryCoordinator mKeyboardAccessory;
    private AccessorySheetCoordinator mAccessorySheet;
    private ChromeActivity mActivity; // Used to control the keyboard.
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private DropdownPopupWindow mPopup;
    private BottomSheetController mBottomSheetController;
    private ManualFillingComponent.SoftKeyboardDelegate mSoftKeyboardDelegate;
    private ConfirmationDialogHelper mConfirmationHelper;

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onHidden(Tab tab, @TabHidingType int type) {
            pause();
        }

        @Override
        public void onDestroyed(Tab tab) {
            mStateCache.destroyStateFor(tab);
            pause();
            refreshTabs();
        }
    };

    private final FullscreenManager.Observer mFullscreenObserver =
            new FullscreenManager.Observer() {
                @Override
                public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                    pause();
                }
            };

    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetStateChanged(@SheetState int newState) {
            mModel.set(SUPPRESSED_BY_BOTTOM_SHEET, newState != SheetState.HIDDEN);
        }
    };

    /** Default constructor */
    ManualFillingMediator() {
        mViewportInsetSupplier.set(0);
    }

    void initialize(KeyboardAccessoryCoordinator keyboardAccessory,
            AccessorySheetCoordinator accessorySheet, WindowAndroid windowAndroid,
            BottomSheetController sheetController,
            ManualFillingComponent.SoftKeyboardDelegate keyboardDelegate,
            ConfirmationDialogHelper confirmationHelper) {
        mActivity = (ChromeActivity) windowAndroid.getActivity().get();
        assert mActivity != null;
        mWindowAndroid = windowAndroid;
        mWindowAndroid.getApplicationBottomInsetProvider().addSupplier(mViewportInsetSupplier);
        mKeyboardAccessory = keyboardAccessory;
        mBottomSheetController = sheetController;
        mSoftKeyboardDelegate = keyboardDelegate;
        mConfirmationHelper = confirmationHelper;
        mModel.set(PORTRAIT_ORIENTATION, hasPortraitOrientation());
        mModel.addObserver(this::onPropertyChanged);
        mAccessorySheet = accessorySheet;
        mAccessorySheet.setOnPageChangeListener(mKeyboardAccessory.getOnPageChangeListener());
        mAccessorySheet.setHeight(3
                * mActivity.getResources().getDimensionPixelSize(
                        R.dimen.keyboard_accessory_suggestion_height));
        setInsetObserverViewSupplier(mActivity::getInsetObserverView);
        mActivity.findViewById(android.R.id.content).addOnLayoutChangeListener(this);
        mTabModelObserver = new TabModelSelectorTabModelObserver(mActivity.getTabModelSelector()) {
            @Override
            public void didSelectTab(Tab tab, int type, int lastId) {
                ensureObserverRegistered(tab);
                refreshTabs();
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                super.tabClosureCommitted(tab);
                mObservedTabs.remove(tab);
                tab.removeObserver(mTabObserver); // Fails silently if observer isn't registered.
                mStateCache.destroyStateFor(tab);
            }
        };
        mActivity.getFullscreenManager().addObserver(mFullscreenObserver);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        ensureObserverRegistered(getActiveBrowserTab());
        refreshTabs();
    }

    boolean isInitialized() {
        return mWindowAndroid != null;
    }

    boolean isFillingViewShown(View view) {
        return isInitialized() && !isSoftKeyboardShowing(view) && mKeyboardAccessory.hasActiveTab();
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        if (!isInitialized()) return; // Activity uninitialized or cleaned up already.
        if (mKeyboardAccessory.empty()) return; // Exit early to not affect the layout.
        if (!hasSufficientSpace()) {
            mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            return;
        }
        if (hasPortraitOrientation() != mModel.get(PORTRAIT_ORIENTATION)) {
            mModel.set(PORTRAIT_ORIENTATION, hasPortraitOrientation());
            return;
        }
        restrictAccessorySheetHeight();
        if (!isSoftKeyboardShowing(view)) {
            if (is(WAITING_TO_REPLACE)) mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
            if (is(EXTENDING_KEYBOARD)) mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            // Cancel animations if the keyboard suddenly closes so the bar doesn't linger.
            if (is(HIDDEN)) mKeyboardAccessory.skipClosingAnimationOnce();
            // Layout changes when entering/resizing/leaving MultiWindow. Ensure a consistent state:
            updateKeyboard(mModel.get(KEYBOARD_EXTENSION_STATE));
            return;
        }
        if (is(WAITING_TO_REPLACE)) return;
        mModel.set(KEYBOARD_EXTENSION_STATE,
                mModel.get(SHOW_WHEN_VISIBLE) ? EXTENDING_KEYBOARD : HIDDEN);
    }

    private boolean hasPortraitOrientation() {
        return mWindowAndroid.getDisplay().getRotation() == Surface.ROTATION_0
                || mWindowAndroid.getDisplay().getRotation() == Surface.ROTATION_180;
    }

    void registerSheetDataProvider(@AccessoryTabType int tabType,
            PropertyProvider<KeyboardAccessoryData.AccessorySheetData> dataProvider) {
        if (!isInitialized()) return;
        ManualFillingState state = mStateCache.getStateFor(mActivity.getCurrentWebContents());

        state.wrapSheetDataProvider(tabType, dataProvider);
        AccessorySheetTabCoordinator accessorySheet = getOrCreateSheet(tabType);
        if (accessorySheet == null) return; // Not available or initialized yet.
        accessorySheet.registerDataProvider(state.getSheetDataProvider(tabType));
    }

    void registerAutofillProvider(
            PropertyProvider<AutofillSuggestion[]> autofillProvider, AutofillDelegate delegate) {
        if (!isInitialized()) return;
        if (mKeyboardAccessory == null) return;
        mKeyboardAccessory.registerAutofillProvider(autofillProvider, delegate);
    }

    void registerActionProvider(PropertyProvider<Action[]> actionProvider) {
        if (!isInitialized()) return;
        ManualFillingState state = mStateCache.getStateFor(mActivity.getCurrentWebContents());

        state.wrapActionsProvider(actionProvider, new Action[0]);
        mKeyboardAccessory.registerActionProvider(state.getActionsProvider());
    }

    void destroy() {
        if (!isInitialized()) return;
        pause();
        mWindowAndroid.getApplicationBottomInsetProvider().removeSupplier(mViewportInsetSupplier);
        mActivity.findViewById(android.R.id.content).removeOnLayoutChangeListener(this);
        mTabModelObserver.destroy();
        mStateCache.destroy();
        for (Tab tab : mObservedTabs) tab.removeObserver(mTabObserver);
        mObservedTabs.clear();
        mActivity.getFullscreenManager().removeObserver(mFullscreenObserver);
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mWindowAndroid = null;
        mActivity = null;
    }

    boolean handleBackPress() {
        if (isInitialized()
                && (is(WAITING_TO_REPLACE) || is(REPLACING_KEYBOARD) || is(FLOATING_SHEET))) {
            pause();
            return true;
        }
        return false;
    }

    void dismiss() {
        if (!isInitialized()) return;
        pause();
        ViewGroup contentView = getContentView();
        if (contentView != null) mSoftKeyboardDelegate.hideSoftKeyboardOnly(contentView);
    }

    void notifyPopupOpened(DropdownPopupWindow popup) {
        mPopup = popup;
    }

    void showWhenKeyboardIsVisible() {
        if (!isInitialized()) return;
        mModel.set(SHOW_WHEN_VISIBLE, true);
        if (is(HIDDEN)) mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
    }

    void hide() {
        mModel.set(SHOW_WHEN_VISIBLE, false);
        if (!isInitialized()) return;
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
    }

    void pause() {
        if (!isInitialized()) return;
        mConfirmationHelper.dismiss();
        // When pause is called, the accessory needs to disappear fast since some UI forced it to
        // close (e.g. a scene changed or the screen was turned off).
        mKeyboardAccessory.skipClosingAnimationOnce();
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
    }

    private void onOrientationChange() {
        if (!isInitialized()) return;
        if (ChromeFeatureList.isEnabled(AUTOFILL_KEYBOARD_ACCESSORY) || is(REPLACING_KEYBOARD)
                || is(FLOATING_SHEET)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            // Autofill suggestions are invalidated on rotation. Dismissing all filling UI forces
            // the user to interact with the field they want to edit. This refreshes Autofill.
            if (ChromeFeatureList.isEnabled(AUTOFILL_KEYBOARD_ACCESSORY)) {
                hideSoftKeyboard();
            }
        }
    }

    void resume() {
        if (!isInitialized()) return;
        pause(); // Resuming dismisses the keyboard. Ensure the accessory doesn't linger.
        refreshTabs();
    }

    private boolean hasSufficientSpace() {
        if (mActivity == null) return false;
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null) return false;
        float height = webContents.getHeight(); // getHeight actually returns dip, not Px!
        height += mViewportInsetSupplier.get() / mWindowAndroid.getDisplay().getDipScale();
        return height >= MINIMAL_AVAILABLE_VERTICAL_SPACE
                && webContents.getWidth() >= MINIMAL_AVAILABLE_HORIZONTAL_SPACE;
    }

    private void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey property) {
        assert source == mModel;
        if (property == SHOW_WHEN_VISIBLE) {
            return;
        } else if (property == PORTRAIT_ORIENTATION) {
            onOrientationChange();
            return;
        } else if (property == KEYBOARD_EXTENSION_STATE) {
            TraceEvent.instant("ManualFillingMediator$KeyboardExtensionState",
                    getNameForState(mModel.get(KEYBOARD_EXTENSION_STATE)));
            transitionIntoState(mModel.get(KEYBOARD_EXTENSION_STATE));
            return;
        } else if (property == SUPPRESSED_BY_BOTTOM_SHEET) {
            if (isInitialized() && mModel.get(SUPPRESSED_BY_BOTTOM_SHEET)) {
                mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            }
            return;
        }
        throw new IllegalArgumentException("Unhandled property: " + property);
    }

    /**
     * If preconditions for a state are met, enforce the state's properties and trigger its effects.
     * @param extensionState The {@link KeyboardExtensionState} to transition into.
     */
    private void transitionIntoState(@KeyboardExtensionState int extensionState) {
        if (!meetsStatePreconditions(extensionState)) return;
        TraceEvent.begin("ManualFillingMediator#transitionIntoState");
        enforceStateProperties(extensionState);
        changeBottomControlSpaceForState(extensionState);
        updateKeyboard(extensionState);
        TraceEvent.end("ManualFillingMediator#transitionIntoState");
    }

    /**
     * Checks preconditions for states and redirects to a different state if they are not met.
     * @param extensionState The {@link KeyboardExtensionState} to transition into.
     */
    private boolean meetsStatePreconditions(@KeyboardExtensionState int extensionState) {
        switch (extensionState) {
            case HIDDEN:
                return true;
            case FLOATING_BAR:
                if (isSoftKeyboardShowing(getContentView())) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
                    return false;
                }
                // Intentional fallthrough.
            case EXTENDING_KEYBOARD:
                if (!canExtendKeyboard() || mModel.get(SUPPRESSED_BY_BOTTOM_SHEET)) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
                    return false;
                }
                return true;
            case FLOATING_SHEET:
                if (isSoftKeyboardShowing(getContentView())) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
                    return false;
                }
                // Intentional fallthrough.
            case REPLACING_KEYBOARD:
                if (isSoftKeyboardShowing(getContentView())) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, WAITING_TO_REPLACE);
                    return false; // Wait for the keyboard to disappear before replacing!
                }
                // Intentional fallthrough.
            case WAITING_TO_REPLACE:
                if (!hasSufficientSpace() || mModel.get(SUPPRESSED_BY_BOTTOM_SHEET)) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
                    return false;
                }
                return true;
        }
        throw new IllegalArgumentException(
                "Unhandled transition into state: " + mModel.get(KEYBOARD_EXTENSION_STATE));
    }

    private void enforceStateProperties(@KeyboardExtensionState int extensionState) {
        if (requiresVisibleBar(extensionState)) {
            mKeyboardAccessory.show();
        } else {
            mKeyboardAccessory.dismiss();
        }
        if (extensionState == EXTENDING_KEYBOARD) mKeyboardAccessory.prepareUserEducation();
        if (requiresVisibleSheet(extensionState)) {
            mAccessorySheet.show();
            // TODO(crbug.com/853768): Enable animation that works with sheet (if possible).
            mKeyboardAccessory.skipClosingAnimationOnce();
        } else if (requiresHiddenSheet(extensionState)) {
            mKeyboardAccessory.closeActiveTab();
            mAccessorySheet.hide();
            // The compositor should relayout the view when the sheet is hidden. This is necessary
            // to trigger events that rely on the relayout (like toggling the overview button):
            CompositorViewHolder compositorViewHolder = mActivity.getCompositorViewHolder();
            if (compositorViewHolder != null) {
                // The CompositorViewHolder is null when the activity is in the process of being
                // destroyed which also renders relayouting pointless.
                compositorViewHolder.requestLayout();
            }
        }
    }

    private void updateKeyboard(@KeyboardExtensionState int extensionState) {
        if (isFloating(extensionState)) {
            // Keyboard-bound states are always preferable over floating states. Therefore, trigger
            // a keyboard here. This also allows for smooth transitions, e.g. when closing a sheet:
            // the REPLACING state transitions into FLOATING_SHEET which triggers the keyboard which
            // transitions into the EXTENDING state as soon as the keyboard appeared.
            ViewGroup contentView = getContentView();
            if (contentView != null) mSoftKeyboardDelegate.showSoftKeyboard(contentView);
        } else if (extensionState == WAITING_TO_REPLACE) {
            // In order to give the keyboard time to disappear, hide the keyboard and enter the
            // REPLACING state.
            hideSoftKeyboard();
        }
    }

    private void hideSoftKeyboard() {
        // If there is a keyboard, update the accessory sheet's height and hide the keyboard.
        ViewGroup contentView = getContentView();
        if (contentView == null) return; // Apparently the tab was cleaned up already.
        View rootView = contentView.getRootView();
        if (rootView == null) return;
        mAccessorySheet.setHeight(calculateAccessorySheetHeight(rootView));
        mSoftKeyboardDelegate.hideSoftKeyboardOnly(rootView);
    }

    /**
     * Returns whether the accessory bar can be shown.
     * @return True if the keyboard can (and should) be shown. False otherwise.
     */
    private boolean canExtendKeyboard() {
        if (!mModel.get(SHOW_WHEN_VISIBLE)) return false;

        // When in VR mode, don't extend the keyboard
        if (VrModuleProvider.getDelegate().isInVr()) return false;

        // Don't open the accessory inside the contextual search panel.
        ContextualSearchManager contextualSearch = mActivity.getContextualSearchManager();
        if (contextualSearch != null && contextualSearch.isSearchPanelOpened()) return false;

        // If an accessory sheet was opened, the accessory bar must be visible.
        if (mAccessorySheet.isShown()) return true;

        return hasSufficientSpace(); // Only extend the keyboard, if there is enough space.
    }

    @Override
    public void onChangeAccessorySheet(int tabIndex) {
        if (!isInitialized()) return;
        mAccessorySheet.setActiveTab(tabIndex);
        if (mPopup != null && mPopup.isShowing()) mPopup.dismiss();
        if (is(EXTENDING_KEYBOARD)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
        } else if (is(FLOATING_BAR)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        }
    }

    @Override
    public void onCloseAccessorySheet() {
        if (is(REPLACING_KEYBOARD) || is(WAITING_TO_REPLACE)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_SHEET);
        } else if (is(FLOATING_SHEET)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
        }
    }

    /**
     * Opens the keyboard which implicitly dismisses the sheet. Without open sheet, this is a NoOp.
     */
    void swapSheetWithKeyboard() {
        if (isInitialized() && mAccessorySheet.isShown()) onCloseAccessorySheet();
    }

    void confirmOperation(String title, String message, Runnable confirmedCallback) {
        mConfirmationHelper.showConfirmation(title, message, R.string.ok, confirmedCallback);
    }

    private void changeBottomControlSpaceForState(int extensionState) {
        if (extensionState == WAITING_TO_REPLACE) return; // Don't change yet.
        int newControlsHeight = 0;
        int newControlsOffset = 0;
        if (requiresVisibleBar(extensionState)) {
            newControlsHeight = mActivity.getResources().getDimensionPixelSize(
                    R.dimen.keyboard_accessory_suggestion_height);
        }
        if (requiresVisibleSheet(extensionState)) {
            newControlsHeight += mAccessorySheet.getHeight();
            newControlsOffset += mAccessorySheet.getHeight();
        }
        mKeyboardAccessory.setBottomOffset(newControlsOffset);
        mViewportInsetSupplier.set(newControlsHeight);
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
        Tab tab = getActiveBrowserTab();
        if (tab == null) return null;
        return tab.getContentView();
    }

    /**
     * Shorthand to get the activity tab.
     * @return The currently visible {@link Tab}, if any.
     */
    private @Nullable Tab getActiveBrowserTab() {
        return mActivity.getActivityTabProvider().get();
    }

    /**
     * Registers a {@link TabObserver} to the given {@link Tab} if it hasn't been done yet.
     * Using this function avoid deleting and readding the observer (each O(N)) since the tab does
     * not report whether an observer is registered.
     * @param tab A {@link Tab}. May be the currently active tab which is allowed to be null.
     */
    private void ensureObserverRegistered(@Nullable Tab tab) {
        if (tab == null) return; // No tab given, no observer necessary.
        if (!mObservedTabs.add(tab)) return; // Observer already registered.
        tab.addObserver(mTabObserver);
    }

    private boolean isSoftKeyboardShowing(@Nullable View view) {
        return view != null && mSoftKeyboardDelegate.isSoftKeyboardShowing(mActivity, view);
    }

    /**
     * Uses the keyboard (if available) to determine the height of the accessory sheet.
     * @param rootView Root view of the current content view -- used to estimate the height unless
     *                 the more reliable InsetObserver is available.
     * @return The estimated keyboard height or enough space to display at least three suggestions.
     */
    private @Px int calculateAccessorySheetHeight(View rootView) {
        InsetObserverView insetObserver = mInsetObserverViewSupplier.get();
        int minimalSheetHeight = 3
                * mActivity.getResources().getDimensionPixelSize(
                        R.dimen.keyboard_accessory_suggestion_height);
        int newSheetHeight = insetObserver != null
                ? insetObserver.getSystemWindowInsetsBottom()
                : mSoftKeyboardDelegate.calculateSoftKeyboardHeight(rootView);
        newSheetHeight = Math.max(minimalSheetHeight, newSheetHeight);
        return newSheetHeight;
    }

    /**
     * Double-checks that the accessory sheet height doesn't cover the whole page.
     */
    private void restrictAccessorySheetHeight() {
        if (!is(FLOATING_SHEET) && !is(REPLACING_KEYBOARD)) return;
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null) return;
        float density = mWindowAndroid.getDisplay().getDipScale();
        // The maximal height for the sheet ensures a minimal amount of WebContents space.
        @Px
        int maxHeight = mViewportInsetSupplier.get();
        maxHeight += Math.round(density * webContents.getHeight());
        maxHeight -= Math.round(density * MINIMAL_AVAILABLE_VERTICAL_SPACE);
        if (mAccessorySheet.getHeight() <= maxHeight) return; // Sheet height needs no adjustment!
        mAccessorySheet.setHeight(maxHeight);
        changeBottomControlSpaceForState(mModel.get(KEYBOARD_EXTENSION_STATE));
    }

    private void refreshTabs() {
        if (!isInitialized()) return;
        ManualFillingState state = mStateCache.getStateFor(mActivity.getCurrentWebContents());
        state.notifyObservers();
        KeyboardAccessoryData.Tab[] tabs = state.getTabs();
        mAccessorySheet.setTabs(tabs); // Set the sheet tabs first to invalidate the tabs properly.
        mKeyboardAccessory.setTabs(tabs);
    }

    @VisibleForTesting
    AccessorySheetTabCoordinator getOrCreateSheet(@AccessoryTabType int tabType) {
        if (!canCreateSheet(tabType)) return null;
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null) return null; // There is no active tab or it's being destroyed.
        ManualFillingState state = mStateCache.getStateFor(webContents);
        if (state.getAccessorySheet(tabType) != null) return state.getAccessorySheet(tabType);

        AccessorySheetTabCoordinator sheet = createNewSheet(tabType);
        assert sheet != null : "Cannot create sheet for type " + tabType;

        state.setAccessorySheet(tabType, sheet);
        if (state.getSheetDataProvider(tabType) != null) {
            sheet.registerDataProvider(state.getSheetDataProvider(tabType));
        }
        refreshTabs();
        return sheet;
    }

    private boolean canCreateSheet(@AccessoryTabType int tabType) {
        if (!isInitialized()) return false;
        switch (tabType) {
            case AccessoryTabType.ALL: // Intentional fallthrough.
            case AccessoryTabType.COUNT:
                return false;
            case AccessoryTabType.CREDIT_CARDS: // Intentional fallthrough.
            case AccessoryTabType.ADDRESSES:
                return ChromeFeatureList.isEnabled(AUTOFILL_MANUAL_FALLBACK_ANDROID);
            case AccessoryTabType.PASSWORDS:
                return true;
            case AccessoryTabType.TOUCH_TO_FILL:
                return true;
        }
        return true;
    }

    private AccessorySheetTabCoordinator createNewSheet(@AccessoryTabType int tabType) {
        switch (tabType) {
            case AccessoryTabType.CREDIT_CARDS:
                return new CreditCardAccessorySheetCoordinator(
                        mActivity, mAccessorySheet.getScrollListener());
            case AccessoryTabType.ADDRESSES:
                return new AddressAccessorySheetCoordinator(
                        mActivity, mAccessorySheet.getScrollListener());
            case AccessoryTabType.PASSWORDS:
                return new PasswordAccessorySheetCoordinator(
                        mActivity, mAccessorySheet.getScrollListener());
            case AccessoryTabType.TOUCH_TO_FILL:
                return new TouchToFillSheetCoordinator(
                        mActivity, mAccessorySheet.getScrollListener());
            case AccessoryTabType.ALL: // Intentional fallthrough.
            case AccessoryTabType.COUNT: // Intentional fallthrough.
        }
        return null;
    }

    private boolean isFloating(@KeyboardExtensionState int state) {
        return (state & StateProperty.FLOATING) != 0;
    }

    private boolean requiresVisibleBar(@KeyboardExtensionState int state) {
        return (state & StateProperty.BAR) != 0;
    }

    private boolean requiresVisibleSheet(@KeyboardExtensionState int state) {
        return (state & StateProperty.VISIBLE_SHEET) != 0;
    }

    private boolean requiresHiddenSheet(int state) {
        return (state & StateProperty.HIDDEN_SHEET) != 0;
    }

    private boolean is(@KeyboardExtensionState int state) {
        return mModel.get(KEYBOARD_EXTENSION_STATE) == state;
    }

    private static String getNameForState(@KeyboardExtensionState int state) {
        switch (state) {
            case HIDDEN:
                return "HIDDEN";
            case EXTENDING_KEYBOARD:
                return "EXTENDING_KEYBOARD";
            case REPLACING_KEYBOARD:
                return "REPLACING_KEYBOARD";
            case WAITING_TO_REPLACE:
                return "WAITING_TO_REPLACE";
            case FLOATING_BAR:
                return "FLOATING_BAR";
            case FLOATING_SHEET:
                return "FLOATING_SHEET";
        }
        return null;
    }

    @VisibleForTesting
    void setInsetObserverViewSupplier(Supplier<InsetObserverView> insetObserverViewSupplier) {
        mInsetObserverViewSupplier = insetObserverViewSupplier;
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
    ManualFillingStateCache getStateCacheForTesting() {
        return mStateCache;
    }

    @VisibleForTesting
    PropertyModel getModelForTesting() {
        return mModel;
    }

    @VisibleForTesting
    KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mKeyboardAccessory;
    }
}
