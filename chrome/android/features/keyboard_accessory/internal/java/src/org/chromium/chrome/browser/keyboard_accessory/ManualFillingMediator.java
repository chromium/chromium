// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.IS_FULLSCREEN;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KEYBOARD_EXTENSION_STATE;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.EXTENDING_KEYBOARD;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.FLOATING_BAR;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.FLOATING_SHEET;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.HIDDEN;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.REPLACING_KEYBOARD;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.KeyboardExtensionState.WAITING_TO_REPLACE;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.PORTRAIT_ORIENTATION;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.SHOULD_EXTEND_KEYBOARD;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.SHOW_WHEN_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingProperties.SUPPRESSED_BY_BOTTOM_SHEET;

import android.util.SparseArray;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
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
import org.chromium.chrome.browser.password_manager.ConfirmationDialogHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.DropdownPopupWindow;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.ViewportInsets;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.mojom.VirtualKeyboardMode;

import java.util.HashSet;
import java.util.List;
import java.util.function.BooleanSupplier;

/**
 * This part of the manual filling component manages the state of the manual filling flow depending
 * on the currently shown tab.
 */
class ManualFillingMediator
        implements KeyboardAccessoryCoordinator.BarVisibilityDelegate,
                AccessorySheetCoordinator.SheetVisibilityDelegate,
                View.OnLayoutChangeListener,
                BackPressHandler {
    private static final int MINIMAL_AVAILABLE_VERTICAL_SPACE = 128; // in DP.
    private static final int MINIMAL_AVAILABLE_HORIZONTAL_SPACE = 180; // in DP.

    private SparseArray<AccessorySheetTabCoordinator> mSheets = new SparseArray<>();
    private PropertyModel mModel = ManualFillingProperties.createFillingModel();
    private WindowAndroid mWindowAndroid;
    private ApplicationViewportInsetSupplier mApplicationViewportInsetSupplier;
    private final ObservableSupplierImpl<Integer> mBottomInsetSupplier =
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
    private BackPressManager mBackPressManager;
    private Supplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier = () -> null;
    private BooleanSupplier mIsContextualSearchOpened;
    private final Callback<ViewportInsets> mViewportInsetsObserver = this::onViewportInsetChanged;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<AccessorySheetVisualStateProvider>
            mAccessorySheetVisualStateSupplier = new ObservableSupplierImpl<>();

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
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

                @Override
                public void onVirtualKeyboardModeChanged(
                        Tab tab, @VirtualKeyboardMode.EnumType int mode) {
                    if (isInitialized() && !mKeyboardAccessory.empty()) {
                        updateExtensionStateAndKeyboard(isSoftKeyboardShowing(getContentView()));
                    }
                }

                @Override
                public void onDidFinishNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigation) {
                    if (isInitialized() && !mKeyboardAccessory.empty()) {
                        updateExtensionStateAndKeyboard(isSoftKeyboardShowing(getContentView()));
                    }
                }
            };

    private final FullscreenManager.Observer mFullscreenObserver =
            new FullscreenManager.Observer() {
                @Override
                public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                    // Only if a navbar exists, fullscreen mode behaves like regular chrome. Ignore.
                    mModel.set(IS_FULLSCREEN, !options.showNavigationBar);
                }

                @Override
                public void onExitFullscreen(Tab tab) {
                    mModel.set(IS_FULLSCREEN, false);
                }
            };

    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(@SheetState int newState, int reason) {
                    mModel.set(SUPPRESSED_BY_BOTTOM_SHEET, newState != SheetState.HIDDEN);
                }
            };

    /** Default constructor */
    ManualFillingMediator() {
        mBottomInsetSupplier.set(0);
    }

    void initialize(
            KeyboardAccessoryCoordinator keyboardAccessory,
            AccessorySheetCoordinator accessorySheet,
            WindowAndroid windowAndroid,
            BottomSheetController sheetController,
            BooleanSupplier isContextualSearchOpened,
            BackPressManager backPressManager,
            Supplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            ManualFillingComponent.SoftKeyboardDelegate keyboardDelegate,
            ConfirmationDialogHelper confirmationHelper) {
        mActivity = (ChromeActivity) windowAndroid.getActivity().get();
        assert mActivity != null;
        mWindowAndroid = windowAndroid;
        mKeyboardAccessory = keyboardAccessory;
        mBottomSheetController = sheetController;
        mIsContextualSearchOpened = isContextualSearchOpened;
        mSoftKeyboardDelegate = keyboardDelegate;
        mConfirmationHelper = confirmationHelper;
        mModel.set(PORTRAIT_ORIENTATION, hasPortraitOrientation());
        mModel.addObserver(this::onPropertyChanged);
        mAccessorySheet = accessorySheet;
        mAccessorySheetVisualStateSupplier.set(mAccessorySheet);
        mAccessorySheet.setOnPageChangeListener(mKeyboardAccessory.getOnPageChangeListener());
        mAccessorySheet.setHeight(getIdealSheetHeight());
        mApplicationViewportInsetSupplier = mWindowAndroid.getApplicationBottomInsetSupplier();
        mApplicationViewportInsetSupplier.addObserver(mViewportInsetsObserver);
        mActivity.findViewById(android.R.id.content).addOnLayoutChangeListener(this);
        mBackPressManager = backPressManager;
        mBackPressChangedSupplier.set(shouldHideOnBackPress());
        if (BackPressManager.isEnabled()) {
            mBackPressManager.addHandler(this, Type.MANUAL_FILLING);
        }
        mEdgeToEdgeControllerSupplier = edgeToEdgeControllerSupplier;

        mTabModelObserver =
                new TabModelSelectorTabModelObserver(mActivity.getTabModelSelector()) {
                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        ensureObserverRegistered(tab);
                        refreshTabs();
                    }

                    @Override
                    public void tabClosureCommitted(Tab tab) {
                        super.tabClosureCommitted(tab);
                        mObservedTabs.remove(tab);
                        tab.removeObserver(
                                mTabObserver); // Fails silently if observer isn't registered.
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
        return isInitialized()
                && !isSoftKeyboardShowing(view)
                && (mKeyboardAccessory.hasActiveTab()
                        || (is(WAITING_TO_REPLACE)
                                || is(REPLACING_KEYBOARD)
                                || is(FLOATING_SHEET)));
    }

    ObservableSupplier<Integer> getBottomInsetSupplier() {
        return mBottomInsetSupplier;
    }

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        if (isInitialized() && !mKeyboardAccessory.empty()) {
            updateExtensionStateAndKeyboard(isSoftKeyboardShowing(view));
        }
    }

    private void updateExtensionStateAndKeyboard(boolean isKeyboardShowing) {
        assert isInitialized() : "Activity uninitialized or cleaned up already.";
        assert !mKeyboardAccessory.empty() : "KA is inactive — don't process updates!";
        if (!hasSufficientSpace()) {
            mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            return;
        }
        if (hasPortraitOrientation() != mModel.get(PORTRAIT_ORIENTATION)) {
            mModel.set(PORTRAIT_ORIENTATION, hasPortraitOrientation());
            return;
        }
        restrictAccessorySheetHeight();
        if (!isKeyboardShowing) {
            if (is(WAITING_TO_REPLACE)) {
                mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
            }
            if (is(EXTENDING_KEYBOARD)) {
                mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            }
            // Cancel animations if the keyboard suddenly closes so the bar doesn't linger.
            if (is(HIDDEN)) mKeyboardAccessory.skipClosingAnimationOnce();
            // Layout changes when entering/resizing/leaving MultiWindow. Ensure a consistent state:
            updateKeyboard(mModel.get(KEYBOARD_EXTENSION_STATE));
            return;
        }
        if (is(WAITING_TO_REPLACE)) return;
        mModel.set(
                KEYBOARD_EXTENSION_STATE,
                mModel.get(SHOW_WHEN_VISIBLE) ? EXTENDING_KEYBOARD : HIDDEN);
    }

    private boolean hasPortraitOrientation() {
        return mWindowAndroid.getDisplay().getRotation() == Surface.ROTATION_0
                || mWindowAndroid.getDisplay().getRotation() == Surface.ROTATION_180;
    }

    public void registerSheetUpdateDelegate(
            WebContents webContents, ManualFillingComponent.UpdateAccessorySheetDelegate delegate) {
        if (!isInitialized()) return;
        ManualFillingState state = mStateCache.getStateFor(webContents);
        state.setSheetUpdater(delegate);
    }

    void registerSheetDataProvider(
            WebContents webContents,
            @AccessoryTabType int tabType,
            PropertyProvider<KeyboardAccessoryData.AccessorySheetData> dataProvider) {
        if (!isInitialized()) return;
        ManualFillingState state = mStateCache.getStateFor(webContents);

        state.wrapSheetDataProvider(tabType, dataProvider);
        AccessorySheetTabCoordinator accessorySheet = getOrCreateSheet(webContents, tabType);
        if (accessorySheet == null) return; // Not available or initialized yet.

        if (state.addAvailableTab(accessorySheet.getTab())) {
            accessorySheet.registerDataProvider(state.getSheetDataProvider(tabType));
        }
        refreshTabs();
    }

    void registerAutofillProvider(
            PropertyProvider<List<AutofillSuggestion>> autofillProvider,
            AutofillDelegate delegate) {
        if (!isInitialized()) return;
        if (mKeyboardAccessory == null) return;
        mKeyboardAccessory.registerAutofillProvider(autofillProvider, delegate);
    }

    void registerActionProvider(
            WebContents webContents, PropertyProvider<Action[]> actionProvider) {
        if (!isInitialized()) return;
        ManualFillingState state = mStateCache.getStateFor(webContents);

        state.wrapActionsProvider(actionProvider, new Action[0]);
        mKeyboardAccessory.registerActionProvider(state.getActionsProvider());
    }

    void destroy() {
        if (!isInitialized()) return;
        pause();
        mActivity.findViewById(android.R.id.content).removeOnLayoutChangeListener(this);
        mTabModelObserver.destroy();
        mStateCache.destroy();
        for (Tab tab : mObservedTabs) tab.removeObserver(mTabObserver);
        mObservedTabs.clear();
        mActivity.getFullscreenManager().removeObserver(mFullscreenObserver);
        mApplicationViewportInsetSupplier.removeObserver(mViewportInsetsObserver);
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBackPressChangedSupplier.set(false);
        mBackPressManager.removeHandler(this);
        mBackPressManager = null;
        mWindowAndroid = null;
        mActivity = null;
    }

    boolean onBackPressed() {
        if (shouldHideOnBackPress()) {
            pause();
            return true;
        }
        return false;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    void dismiss() {
        if (!isInitialized()) return;
        pause();
        hideSoftKeyboard();
    }

    void notifyPopupOpened(DropdownPopupWindow popup) {
        mPopup = popup;
    }

    void show(boolean waitForKeyboard) {
        showWithKeyboardExtensionState(waitForKeyboard);
    }

    private void showWithKeyboardExtensionState(boolean shouldExtendKeyboard) {
        if (!isInitialized()) return;
        mModel.set(SHOW_WHEN_VISIBLE, true);
        mModel.set(SHOULD_EXTEND_KEYBOARD, shouldExtendKeyboard);
        if (is(HIDDEN)) mModel.set(KEYBOARD_EXTENSION_STATE, FLOATING_BAR);
    }

    void hide() {
        mModel.set(SHOW_WHEN_VISIBLE, false);
        if (!isInitialized()) return;
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
    }

    void showAccessorySheetTab(@AccessoryTabType int tabType) {
        if (!isInitialized()) {
            return;
        }
        mModel.set(SHOW_WHEN_VISIBLE, true);
        if (is(HIDDEN)) {
            mModel.set(KEYBOARD_EXTENSION_STATE, REPLACING_KEYBOARD);
        }
        mKeyboardAccessory.setActiveTab(tabType);
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
        mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
        // Autofill suggestions are invalidated on rotation. Dismissing all filling UI forces
        // the user to interact with the field they want to edit. This refreshes Autofill.
        hideSoftKeyboard();
    }

    void resume() {
        if (!isInitialized()) return;
        pause(); // Resuming dismisses the keyboard. Ensure the accessory doesn't linger.
        refreshTabs();
    }

    private boolean hasSufficientSpace() {
        if (mActivity == null) return false;
        // TODO(bokan): Once mApplicationViewportInsetSupplier includes browser controls, we can use
        // CompositorViewHolder instead of WebContents and simply apply the viewVisibleHeightInset
        // to it, rather than awkwardly undoing the webContentsHeightInset.
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null || webContents.isDestroyed()) return false;
        float height = webContents.getHeight(); // In dip. Already insetted by top/bottom controls.

        // Un-inset the keyboard-related WebContents inset to get back to the CompositorViewHolder
        // viewport height (minus browser controls). This will correctly account for the virtual
        // keyboard mode which is baked into webContents' height. The CompositorViewHolder is
        // resized by the soft keyboard. Don't consider the impact of the accessory as
        // shown already. If we have space for a bar, we continue to have it. The sheet is never
        // bigger than an open keyboard — so if an open sheet affects the inset, we can safely
        // ignore it, too.
        height +=
                mApplicationViewportInsetSupplier.get().webContentsHeightInset
                        / mWindowAndroid.getDisplay().getDipScale();

        return height >= MINIMAL_AVAILABLE_VERTICAL_SPACE // Allows for a bar if not shown yet.
                && webContents.getWidth() >= MINIMAL_AVAILABLE_HORIZONTAL_SPACE;
    }

    private void onPropertyChanged(PropertyObservable<PropertyKey> source, PropertyKey property) {
        assert source == mModel;
        mBackPressChangedSupplier.set(shouldHideOnBackPress());
        if (property == SHOW_WHEN_VISIBLE) {
            return;
        } else if (property == IS_FULLSCREEN) {
            if (isInitialized() && !mKeyboardAccessory.empty()) {
                updateExtensionStateAndKeyboard(isSoftKeyboardShowing(getContentView()));
                changeBottomControlSpaceForState(mModel.get(KEYBOARD_EXTENSION_STATE));
            }
            return;
        } else if (property == PORTRAIT_ORIENTATION) {
            onOrientationChange();
            return;
        } else if (property == KEYBOARD_EXTENSION_STATE) {
            TraceEvent.instant(
                    "ManualFillingMediator$KeyboardExtensionState",
                    getNameForState(mModel.get(KEYBOARD_EXTENSION_STATE)));
            transitionIntoState(mModel.get(KEYBOARD_EXTENSION_STATE));
            return;
        } else if (property == SUPPRESSED_BY_BOTTOM_SHEET) {
            if (isInitialized() && mModel.get(SUPPRESSED_BY_BOTTOM_SHEET)) {
                mModel.set(KEYBOARD_EXTENSION_STATE, HIDDEN);
            }
            return;
        } else if (property == SHOULD_EXTEND_KEYBOARD) {
            // Do nothing. SHOULD_EXTEND_KEYBOARD is used with KEYBOARD_EXTENSION_STATE.
            // However, if SHOULD_EXTEND_KEYBOARD is changed to false, keyboard accessory should be
            // in HIDDEN state.
            assert mModel.get(SHOULD_EXTEND_KEYBOARD) || is(HIDDEN);
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
        changeBottomControlSpaceForState(extensionState);
        enforceStateProperties(extensionState); // Triggers a relayout. Call after changing insets.
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
                if (mModel.get(SHOULD_EXTEND_KEYBOARD) && isSoftKeyboardShowing(getContentView())) {
                    mModel.set(KEYBOARD_EXTENSION_STATE, EXTENDING_KEYBOARD);
                    return false;
                }
                if (!mModel.get(SHOULD_EXTEND_KEYBOARD)) return true;
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
        TraceEvent.begin("ManualFillingMediator#enforceStateProperties");
        if (requiresVisibleBar(extensionState)) {
            mKeyboardAccessory.show();
        } else {
            mKeyboardAccessory.dismiss();
        }
        if (requiresVisibleSheet(extensionState)) {
            mAccessorySheet.show();
            // TODO(crbug.com/40581202): Enable animation that works with sheet (if possible).
            mKeyboardAccessory.skipClosingAnimationOnce();
        } else if (requiresHiddenSheet(extensionState)) {
            mKeyboardAccessory.closeActiveTab();
            mAccessorySheet.hide();
            // The compositor should relayout the view when the sheet is hidden. This is necessary
            // to trigger events that rely on the relayout (like toggling the overview button):
            Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                    mActivity.getCompositorViewHolderSupplier();
            if (compositorViewHolderSupplier.hasValue()) {
                // The CompositorViewHolder is null when the activity is in the process of being
                // destroyed which also renders relayouting pointless.
                ViewUtils.requestLayout(
                        compositorViewHolderSupplier.get(),
                        "ManualFillingMediator.enforceStateProperties");
            }
            trySetA11yFocusOnWebContents();
        }
        TraceEvent.end("ManualFillingMediator#enforceStateProperties");
    }

    private void updateKeyboard(@KeyboardExtensionState int extensionState) {
        if (!mModel.get(SHOULD_EXTEND_KEYBOARD)) return;
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
        mAccessorySheet.setHeight(calculateAccessorySheetHeight());
        mSoftKeyboardDelegate.hideSoftKeyboardOnly(rootView);
    }

    /**
     * Returns whether the accessory bar can be shown.
     * @return True if the keyboard can (and should) be shown. False otherwise.
     */
    private boolean canExtendKeyboard() {
        if (!mModel.get(SHOW_WHEN_VISIBLE)) return false;

        // Don't open the accessory inside the contextual search panel.
        if (mIsContextualSearchOpened.getAsBoolean()) {
            return false;
        }

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

    @Override
    public void onBarFadeInAnimationEnd() {
        if (mActivity != null && mActivity.getCurrentWebContents() != null) {
            mActivity.getCurrentWebContents().scrollFocusedEditableNodeIntoView();
        }
    }

    /** Returns the amount that the keyboard will be extended by the accessory bar. */
    public int getKeyboardExtensionHeight() {
        if (!canExtendKeyboard()) return 0;

        return mActivity
                .getResources()
                .getDimensionPixelSize(R.dimen.keyboard_accessory_suggestion_height);
    }

    /**
     * Opens the keyboard which implicitly dismisses the sheet. Without open sheet, this is a NoOp.
     */
    void swapSheetWithKeyboard() {
        if (isInitialized() && mAccessorySheet.isShown()) onCloseAccessorySheet();
    }

    void confirmOperation(
            String title, String message, Runnable confirmedCallback, Runnable declinedCallback) {
        mConfirmationHelper.showConfirmation(
                title, message, R.string.ok, confirmedCallback, declinedCallback);
    }

    private void changeBottomControlSpaceForState(int extensionState) {
        if (extensionState == WAITING_TO_REPLACE) return; // Don't change yet.
        int newControlsHeight = 0;
        int newControlsOffset = 0;
        if (requiresVisibleBar(extensionState)) {
            boolean isEdgeToEdgeActive = mEdgeToEdgeControllerSupplier.get() != null;
            // TODO(crbug.com/41483806): Treat VirtualKeyboardMode.OVERLAYS_CONTENT like fullscreen?
            if (mModel.get(IS_FULLSCREEN) // Hides UI and lets keyboard overlay webContents.
                    // No need to set the controls height to 0 in edge-to-edge since the content
                    // view will resize to account for the keyboard.
                    && !isEdgeToEdgeActive) {
                newControlsOffset = getKeyboardAndNavigationHeight();
                // Don't resize the page because the keyboard does not doesn't do that either in
                // fullscreen mode. It's overlaying the content and the accessory mimics that.
                newControlsHeight = 0;
            } else {
                newControlsHeight = getBarHeightWithoutShadow();
            }
        }
        if (requiresVisibleSheet(extensionState)) {
            newControlsHeight += mAccessorySheet.getHeight();
            newControlsOffset += mAccessorySheet.getHeight();
        }
        mKeyboardAccessory.setBottomOffset(newControlsOffset);
        mBottomInsetSupplier.set(newControlsHeight);
    }

    private void onViewportInsetChanged(ViewportInsets newViewportInsets) {
        if (isInitialized() && !mKeyboardAccessory.empty()) {
            updateExtensionStateAndKeyboard(isSoftKeyboardShowing(getContentView()));
        }
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
     * @return {@link WebContentsAccessibility} instance of the active tab, if available.
     */
    private @Nullable WebContentsAccessibility getActiveWebContentsAccessibility() {
        if (!ChromeAccessibilityUtil.get().isAccessibilityEnabled()) return null;

        Tab tab = getActiveBrowserTab();
        if (tab == null) return null;

        WebContents webContents = tab.getWebContents();
        if (webContents == null) return null;

        return WebContentsAccessibility.fromWebContents(webContents);
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
     *
     * @return The estimated keyboard height or enough space to display at least three suggestions.
     */
    private @Px int calculateAccessorySheetHeight() {
        int minimalSheetHeight = getIdealSheetHeight();
        int newSheetHeight = getKeyboardAndNavigationHeight() + getHeaderHeight();
        return Math.max(newSheetHeight, minimalSheetHeight);
    }

    /**
     * Uses window insets to estimate the keyboard height. It should be the same mechanism that the
     * SoftKeyboardDelegate uses but it intentionally ignores system navigation elements.
     *
     * @return The estimated keyboard height including the navigation buttons/gesture bar.
     */
    private @Px int getKeyboardAndNavigationHeight() {
        if (getContentView() != null && getContentView().getRootWindowInsets() != null) {
            return WindowInsetsCompat.toWindowInsetsCompat(
                            getContentView().getRootWindowInsets(), getContentView())
                    .getInsets(WindowInsetsCompat.Type.ime())
                    .bottom;
        }
        // If the insets don't work, use the defaults of the KeyboardDelegate. Now, it uses the same
        // WindowInsetsCompat methods so the following line almost certainly means the height is 0.
        return mSoftKeyboardDelegate.calculateSoftKeyboardHeight(getContentView());
    }

    /** Double-checks that the accessory sheet height doesn't cover the whole page. */
    private void restrictAccessorySheetHeight() {
        if (!is(FLOATING_SHEET) && !is(REPLACING_KEYBOARD)) return;
        // TODO(bokan): Once mApplicationViewportInsetSupplier includes browser controls, we can use
        // CompositorViewHolder instead of WebContents and simply apply the viewVisibleHeightInset
        // to it, rather than awkwardly undoing the webContentsHeightInset.
        WebContents webContents = mActivity.getCurrentWebContents();
        if (webContents == null || webContents.isDestroyed()) return;
        float density = mWindowAndroid.getDisplay().getDipScale();
        // Ensure the sheet height is adjusted, if needed, to leave a minimal amount of WebContents
        // space.
        @Px int visibleViewportHeightPx = Math.round(density * webContents.getHeight());

        // Un-inset the keyboard-related WebContents inset to get back to the CompositorViewHolder
        // viewport height (minus browser controls). This will correctly account for the virtual
        // keyboard mode which is baked into webContents' height. The CompositorViewHolder is
        // resized by the soft keyboard.
        visibleViewportHeightPx += mApplicationViewportInsetSupplier.get().webContentsHeightInset;

        // Now remove the insets coming from this class. This will already include the sheet height.
        visibleViewportHeightPx -= mBottomInsetSupplier.get();

        int minimumVerticalSpacePx = Math.round(density * MINIMAL_AVAILABLE_VERTICAL_SPACE);

        // TODO(crbug.com/40285164): google-java-format did not introduce '{}'s as expected in the
        // if
        // construct below (see crbug.com/1505284 for failure). Investigate why and fix it or file a
        // corresponding bug.
        if (visibleViewportHeightPx >= minimumVerticalSpacePx) {
            return; // Sheet height needs no adjustment!
        }

        // Adjust the height such that the new visible height will be exactly
        // MINIMAL_AVAILABLE_VERTICAL_SPACE.
        mAccessorySheet.setHeight(
                visibleViewportHeightPx + mAccessorySheet.getHeight() - minimumVerticalSpacePx);
        changeBottomControlSpaceForState(mModel.get(KEYBOARD_EXTENSION_STATE));
    }

    private void refreshTabs() {
        if (!isInitialized()) return;
        TraceEvent.begin("ManualFillingMediator#refreshTabs");
        ManualFillingState state = mStateCache.getStateFor(mActivity.getCurrentWebContents());
        state.notifyObservers();
        KeyboardAccessoryData.Tab[] tabs = state.getTabs();
        mAccessorySheet.setTabs(tabs); // Set the sheet tabs first to invalidate the tabs properly.
        mKeyboardAccessory.setTabs(tabs);
        state.requestRecentSheets();
        TraceEvent.end("ManualFillingMediator#refreshTabs");
    }

    @VisibleForTesting
    AccessorySheetTabCoordinator getOrCreateSheet(
            WebContents webContents, @AccessoryTabType int tabType) {
        if (!canCreateSheet(tabType) || webContents.isDestroyed()) return null;
        AccessorySheetTabCoordinator sheet;
        ManualFillingState state = mStateCache.getStateFor(webContents);
        sheet = mSheets.get(tabType, null);
        if (sheet != null) return sheet;
        sheet = createNewSheet(Profile.fromWebContents(webContents), tabType);

        mSheets.put(tabType, sheet);
        if (state.getSheetDataProvider(tabType) != null) {
            if (state.addAvailableTab(sheet.getTab())) {
                sheet.registerDataProvider(state.getSheetDataProvider(tabType));
            }
        }
        return sheet;
    }

    private boolean canCreateSheet(@AccessoryTabType int tabType) {
        if (!isInitialized()) return false;
        switch (tabType) {
            case AccessoryTabType.CREDIT_CARDS:
            case AccessoryTabType.ADDRESSES:
            case AccessoryTabType.PASSWORDS:
                return true;
            case AccessoryTabType.OBSOLETE_TOUCH_TO_FILL:
                assert false : "Obsolete sheet type: " + tabType;
                return false;
            case AccessoryTabType.ALL: // Intentional fallthrough.
            case AccessoryTabType.COUNT: // Intentional fallthrough.
        }
        assert false : "Unhandled sheet type: " + tabType;
        return false;
    }

    private AccessorySheetTabCoordinator createNewSheet(
            Profile profile, @AccessoryTabType int tabType) {
        switch (tabType) {
            case AccessoryTabType.CREDIT_CARDS:
                return new CreditCardAccessorySheetCoordinator(
                        mActivity, profile, mAccessorySheet.getScrollListener());
            case AccessoryTabType.ADDRESSES:
                return new AddressAccessorySheetCoordinator(
                        mActivity, profile, mAccessorySheet.getScrollListener());
            case AccessoryTabType.PASSWORDS:
                return new PasswordAccessorySheetCoordinator(
                        mActivity, profile, mAccessorySheet.getScrollListener());
            case AccessoryTabType.OBSOLETE_TOUCH_TO_FILL:
            case AccessoryTabType.ALL: // Intentional fallthrough.
            case AccessoryTabType.COUNT: // Intentional fallthrough.
        }
        assert false : "Cannot create sheet for type " + tabType;
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

    private boolean shouldHideOnBackPress() {
        return isInitialized()
                && (is(WAITING_TO_REPLACE) || is(REPLACING_KEYBOARD) || is(FLOATING_SHEET));
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

    private void trySetA11yFocusOnWebContents() {
        WebContentsAccessibility accessibility = getActiveWebContentsAccessibility();
        if (accessibility != null) {
            accessibility.restoreFocus();
        }
    }

    private @Px int getBarHeightWithoutShadow() {
        return mActivity
                .getResources()
                .getDimensionPixelSize(R.dimen.keyboard_accessory_suggestion_height);
    }

    private @Px int getHeaderHeight() {
        return mActivity
                .getResources()
                .getDimensionPixelSize(R.dimen.keyboard_accessory_height_with_shadow);
    }

    private @Px int getIdealSheetHeight() {
        int idealHeight =
                3
                        * mActivity
                                .getResources()
                                .getDimensionPixelSize(
                                        R.dimen.keyboard_accessory_suggestion_height);
        return idealHeight + getHeaderHeight();
    }

    /**
     * Returns a supplier for {@link AccessorySheetVisualStateProvider} that can be observed to be
     * notified of changes to the visual state of the accessory sheet.
     */
    ObservableSupplier<AccessorySheetVisualStateProvider> getAccessorySheetVisualStateProvider() {
        return mAccessorySheetVisualStateSupplier;
    }

    TabModelObserver getTabModelObserverForTesting() {
        return mTabModelObserver;
    }

    TabObserver getTabObserverForTesting() {
        return mTabObserver;
    }

    ManualFillingStateCache getStateCacheForTesting() {
        return mStateCache;
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    @VisibleForTesting
    KeyboardAccessoryCoordinator getKeyboardAccessory() {
        return mKeyboardAccessory;
    }
}
