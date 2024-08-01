// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupUtils;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * A coordinator for TabGroupUi component. Manages the communication with {@link TabListCoordinator}
 * as well as the life-cycle of shared component objects.
 */
public class TabGroupUiCoordinator
        implements TabGroupUiMediator.ResetHandler,
                TabGroupUi,
                TabGroupUiMediator.TabGroupUiController {
    static final String COMPONENT_NAME = "TabStrip";

    /** Set by {@code mMediator}, but owned by the coordinator so access is safe pre-native. */
    private final ObservableSupplierImpl<Boolean> mHandleBackPressChangedSupplier =
            new ObservableSupplierImpl<>();

    private final Activity mActivity;
    private final Context mContext;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final PropertyModel mModel;
    private final IncognitoStateProvider mIncognitoStateProvider;
    private final TabGroupUiToolbarView mToolbarView;
    private final ViewGroup mTabListContainerView;
    private final ScrimCoordinator mScrimCoordinator;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final BottomSheetController mBottomSheetController;
    private final ViewGroup mRootView;
    private final TabModelSelector mTabModelSelector;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final SnackbarManager mSnackbarManager;
    private final TabCreatorManager mTabCreatorManager;
    private final TabContentManager mTabContentManager;
    private final ModalDialogManager mModalDialogManager;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private TabGridDialogCoordinator mTabGridDialogCoordinator;
    private LazyOneshotSupplierImpl<TabGridDialogMediator.DialogController>
            mTabGridDialogControllerSupplier;
    private TabListCoordinator mTabStripCoordinator;
    private TabGroupUiMediator mMediator;

    /** Creates a new {@link TabGroupUiCoordinator} */
    public TabGroupUiCoordinator(
            @NonNull Activity activity,
            @NonNull ViewGroup parentView,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull IncognitoStateProvider incognitoStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull ViewGroup rootView,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager) {
        try (TraceEvent e = TraceEvent.scoped("TabGroupUiCoordinator.constructor")) {
            mActivity = activity;
            mContext = parentView.getContext();
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mIncognitoStateProvider = incognitoStateProvider;
            mScrimCoordinator = scrimCoordinator;
            mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
            mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);
            mToolbarView =
                    (TabGroupUiToolbarView)
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.bottom_tab_strip_toolbar, parentView, false);
            mTabListContainerView = mToolbarView.getViewContainer();
            mBottomSheetController = bottomSheetController;
            mTabModelSelector = tabModelSelector;
            mLayoutStateProviderSupplier = layoutStateProviderSupplier;
            mRootView = rootView;
            mSnackbarManager = snackbarManager;
            mTabCreatorManager = tabCreatorManager;
            mTabContentManager = tabContentManager;
            mModalDialogManager = modalDialogManager;
            parentView.addView(mToolbarView);
        }
    }

    private void initTabGridDialogCoordinator() {
        assert mTabGridDialogControllerSupplier != null;
        if (mTabGridDialogCoordinator != null) return;

        var currentTabModelFilterSupplier =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilterSupplier();
        Profile profile = mTabModelSelector.getModel(false).getProfile();
        TabGroupModelFilter filter = (TabGroupModelFilter) currentTabModelFilterSupplier.get();
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(profile, mActivity, filter, mModalDialogManager);

        mTabGridDialogCoordinator =
                new TabGridDialogCoordinator(
                        mActivity,
                        mBrowserControlsStateProvider,
                        mBottomSheetController,
                        currentTabModelFilterSupplier,
                        mTabContentManager,
                        mTabCreatorManager,
                        mActivity.findViewById(R.id.coordinator),
                        null,
                        null,
                        null,
                        mScrimCoordinator,
                        mTabStripCoordinator.getTabGroupTitleEditor(),
                        mRootView,
                        actionConfirmationManager,
                        mModalDialogManager);
        mTabGridDialogControllerSupplier.set(mTabGridDialogCoordinator);
    }

    /** Handle any initialization that occurs once native has been loaded. */
    @Override
    public void initializeWithNative(
            Activity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange) {
        var currentTabModelFilterSupplier =
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilterSupplier();
        try (TraceEvent e = TraceEvent.scoped("TabGroupUiCoordinator.initializeWithNative")) {
            mTabStripCoordinator =
                    new TabListCoordinator(
                            TabListCoordinator.TabListMode.STRIP,
                            mContext,
                            mBrowserControlsStateProvider,
                            mModalDialogManager,
                            currentTabModelFilterSupplier,
                            /* thumbnailProvider= */ null,
                            /* actionOnRelatedTabs= */ false,
                            /* gridCardOnClickListenerProvider= */ null,
                            /* dialogHandler= */ null,
                            TabProperties.TabActionState.UNSET,
                            /* selectionDelegateProvider= */ null,
                            /* priceWelcomeMessageControllerSupplier= */ null,
                            mTabListContainerView,
                            /* attachToParent= */ true,
                            COMPONENT_NAME,
                            onModelTokenChange,
                            /* allowDragAndDrop= */ false);
            mTabStripCoordinator.initWithNative(mTabModelSelector.getModel(false).getProfile());

            mModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel,
                            new TabGroupUiViewBinder.ViewHolder(
                                    mToolbarView, mTabStripCoordinator.getContainerView()),
                            TabGroupUiViewBinder::bind);

            // TODO(crbug.com/40631286): find a way to enable interactions between grid tab switcher
            //  and the dialog here.
            if (mScrimCoordinator != null) {
                mTabGridDialogControllerSupplier =
                        new LazyOneshotSupplierImpl<>() {
                            @Override
                            public void doSet() {
                                initTabGridDialogCoordinator();
                            }
                        };
            } else {
                mTabGridDialogControllerSupplier = null;
            }

            mMediator =
                    new TabGroupUiMediator(
                            mActivity,
                            visibilityController,
                            mHandleBackPressChangedSupplier,
                            this,
                            mModel,
                            mTabModelSelector,
                            mTabCreatorManager,
                            mLayoutStateProviderSupplier,
                            mIncognitoStateProvider,
                            mTabGridDialogControllerSupplier,
                            mOmniboxFocusStateSupplier);

            TabGroupUtils.startObservingForCreationIPH();
        }
    }

    /**
     * @return {@link Supplier} that provides dialog visibility.
     */
    @Override
    public boolean isTabGridDialogVisible() {
        return mTabGridDialogCoordinator != null && mTabGridDialogCoordinator.isVisible();
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator} to reset the tab strip.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetStripWithListOfTabs(List<Tab> tabs) {
        if (tabs != null
                && mBottomSheetController.getSheetState()
                        == BottomSheetController.SheetState.HIDDEN) {
            TabGroupUtils.maybeShowIPH(
                    mTabModelSelector.getModel(false).getProfile(),
                    FeatureConstants.TAB_GROUPS_TAP_TO_SEE_ANOTHER_TAB_FEATURE,
                    mTabStripCoordinator.getContainerView(),
                    mBottomSheetController);
        }
        mTabStripCoordinator.resetWithListOfTabs(tabs, false);
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator} when the bottom sheet is
     * expanded or the dialog is shown.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetGridWithListOfTabs(List<Tab> tabs) {
        if (mTabGridDialogControllerSupplier != null) {
            mTabGridDialogControllerSupplier.get().resetWithListOfTabs(tabs);
        }
    }

    /** TabGroupUi implementation. */
    @Override
    public boolean onBackPressed() {
        if (mMediator == null) return false;
        return mMediator.onBackPressed();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        if (mMediator == null) return BackPressResult.FAILURE;
        return mMediator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    /** Destroy any members that needs clean up. */
    @Override
    public void destroy() {
        // TODO(crbug.com/40766050): Add tests for destroy conditions.
        // Early return if the component hasn't initialized yet.
        if (mActivity == null) return;

        mTabStripCoordinator.onDestroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mModelChangeProcessor.destroy();
        if (mMediator != null) {
            mMediator.destroy();
        }
    }

    // TabGroupUiController implementation.
    @Override
    public void setupLeftButtonDrawable(int drawableId) {
        assert mMediator != null;
        mMediator.setupLeftButtonDrawable(drawableId);
    }

    @Override
    public void setupLeftButtonOnClickListener(View.OnClickListener listener) {
        assert mMediator != null;
        mMediator.setupLeftButtonOnClickListener(listener);
    }
}
