// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabKeyEventHandler.onPageKeyEvent;

import android.app.Activity;
import android.content.res.Resources;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.LayoutRes;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesConfig;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.SingleChildViewManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGridDialogMediator.DialogController;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;
import java.util.function.Supplier;

/**
 * A coordinator for TabGroupUi component. Manages the communication with {@link TabListCoordinator}
 * as well as the life-cycle of shared component objects.
 */
@NullMarked
public class TabGroupUiCoordinator implements TabGroupUiMediator.ResetHandler, TabGroupUi {
    static final String COMPONENT_NAME = "TabStrip";

    /** Set by {@code mMediator}, but owned by the coordinator so access is safe pre-native. */
    private final ObservableSupplierImpl<Boolean> mHandleBackPressChangedSupplier =
            new ObservableSupplierImpl<>();

    private final Activity mActivity;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final PropertyModel mModel;
    private final TabGroupUiToolbarView mToolbarView;
    private final ViewGroup mTabListContainerView;
    private final ScrimManager mScrimManager;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final BottomSheetController mBottomSheetController;
    private final DataSharingTabManager mDataSharingTabManager;
    private final TabModelSelector mTabModelSelector;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final TabCreatorManager mTabCreatorManager;
    private final TabContentManager mTabContentManager;
    private final ModalDialogManager mModalDialogManager;
    private final ObservableSupplierImpl<@Nullable Token> mCurrentTabGroupId =
            new ObservableSupplierImpl<>();
    private final ThemeColorProvider mThemeColorProvider;
    private final UndoBarThrottle mUndoBarThrottle;
    private final ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;

    private @Nullable PropertyModelChangeProcessor mModelChangeProcessor;
    private @Nullable TabGridDialogCoordinator mTabGridDialogCoordinator;
    private @Nullable SingleChildViewManager mSingleChildViewManager;
    private @Nullable LazyOneshotSupplier<DialogController> mTabGridDialogControllerSupplier;
    private @Nullable TabListCoordinator mTabStripCoordinator;
    private @Nullable TabGroupUiMediator mMediator;
    private @Nullable TabBubbler mTabBubbler;

    /** Creates a new {@link TabGroupUiCoordinator} */
    public TabGroupUiCoordinator(
            Activity activity,
            ViewGroup parentView,
            BrowserControlsStateProvider browserControlsStateProvider,
            ScrimManager scrimManager,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            BottomSheetController bottomSheetController,
            DataSharingTabManager dataSharingTabManager,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            ModalDialogManager modalDialogManager,
            ThemeColorProvider themeColorProvider,
            UndoBarThrottle undoBarThrottle,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        try (TraceEvent e = TraceEvent.scoped("TabGroupUiCoordinator.constructor")) {
            mActivity = activity;
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mScrimManager = scrimManager;
            mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
            mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);

            @LayoutRes
            int layoutId =
                    TabUiUtils.isDataSharingFunctionalityEnabled()
                            ? R.layout.dynamic_bottom_tab_strip_toolbar
                            : R.layout.bottom_tab_strip_toolbar;
            mToolbarView =
                    (TabGroupUiToolbarView)
                            LayoutInflater.from(activity).inflate(layoutId, parentView, false);
            mTabListContainerView = mToolbarView.getViewContainer();
            mBottomSheetController = bottomSheetController;
            mDataSharingTabManager = dataSharingTabManager;
            mTabModelSelector = tabModelSelector;
            mLayoutStateProviderSupplier = layoutStateProviderSupplier;
            mTabCreatorManager = tabCreatorManager;
            mTabContentManager = tabContentManager;
            mModalDialogManager = modalDialogManager;
            mThemeColorProvider = themeColorProvider;
            mUndoBarThrottle = undoBarThrottle;
            mTabBookmarkerSupplier = tabBookmarkerSupplier;
            mShareDelegateSupplier = shareDelegateSupplier;
            parentView.addView(mToolbarView);
        }
    }

    private DialogController initTabGridDialogCoordinator() {
        assert mTabGridDialogControllerSupplier != null;
        if (mTabGridDialogCoordinator != null) return mTabGridDialogCoordinator;

        ViewGroup containerView = mActivity.findViewById(R.id.coordinator);
        ViewGroup dialogContainer = containerView.findViewById(R.id.tab_group_ui_dialog_container);

        var currentTabGroupModelFilterSupplier =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getCurrentTabGroupModelFilterSupplier();
        ObservableSupplierImpl<@Nullable View> childViewSupplier = new ObservableSupplierImpl<>();
        mSingleChildViewManager = new SingleChildViewManager(dialogContainer, childViewSupplier);
        mTabGridDialogCoordinator =
                new TabGridDialogCoordinator(
                        mActivity,
                        mBrowserControlsStateProvider,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        currentTabGroupModelFilterSupplier,
                        mTabContentManager,
                        null,
                        null,
                        null,
                        mScrimManager,
                        mModalDialogManager,
                        /* desktopWindowStateManager= */ null,
                        mUndoBarThrottle,
                        mTabBookmarkerSupplier,
                        mShareDelegateSupplier,
                        (view) -> childViewSupplier.set(assumeNonNull(view)));
        mTabGridDialogCoordinator.setPageKeyEvent(
                event ->
                        onPageKeyEvent(
                                event,
                                assumeNonNull(currentTabGroupModelFilterSupplier.get()),
                                /* moveSingleTab= */ true));
        return mTabGridDialogCoordinator;
    }

    /** Handle any initialization that occurs once native has been loaded. */
    @Override
    @Initializer
    public void initializeWithNative(
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            Callback<Object> onSnapshotTokenChange) {
        ObservableSupplierImpl<Object> tabStripTokenSupplier = new ObservableSupplierImpl<>();

        var currentTabGroupModelFilterSupplier =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getCurrentTabGroupModelFilterSupplier();
        try (TraceEvent e = TraceEvent.scoped("TabGroupUiCoordinator.initializeWithNative")) {
            mTabStripCoordinator =
                    new TabListCoordinator(
                            TabListCoordinator.TabListMode.STRIP,
                            mActivity,
                            mBrowserControlsStateProvider,
                            mModalDialogManager,
                            currentTabGroupModelFilterSupplier,
                            /* thumbnailProvider= */ null,
                            /* actionOnRelatedTabs= */ false,
                            mDataSharingTabManager,
                            /* gridCardOnClickListenerProvider= */ null,
                            /* dialogHandler= */ null,
                            TabProperties.TabActionState.UNSET,
                            /* selectionDelegateProvider= */ null,
                            /* priceWelcomeMessageControllerSupplier= */ null,
                            mTabListContainerView,
                            /* attachToParent= */ true,
                            COMPONENT_NAME,
                            tabStripTokenSupplier::set,
                            /* emptyViewParent= */ null,
                            /* emptyImageResId= */ Resources.ID_NULL,
                            /* emptyHeadingStringResId= */ Resources.ID_NULL,
                            /* emptySubheadingStringResId= */ Resources.ID_NULL,
                            /* onTabGroupCreation= */ null,
                            /* allowDragAndDrop= */ false,
                            /* tabSwitcherDragHandler= */ null,
                            /* undoBarExplicitTrigger= */ null,
                            null,
                            0);
            mTabStripCoordinator.initWithNative(
                    assumeNonNull(mTabModelSelector.getModel(false).getProfile()));

            mModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel,
                            new TabGroupUiViewBinder.ViewHolder(
                                    mToolbarView, mTabStripCoordinator.getContainerView()),
                            TabGroupUiViewBinder::bind);

            // TODO(crbug.com/40631286): find a way to enable interactions between grid tab switcher
            //  and the dialog here.
            if (mScrimManager != null) {
                mTabGridDialogControllerSupplier =
                        LazyOneshotSupplier.fromSupplier(this::initTabGridDialogCoordinator);
            } else {
                mTabGridDialogControllerSupplier = null;
            }

            SharedImageTilesCoordinator sharedImageTilesCoordinator = null;
            SharedImageTilesConfig.Builder sharedImageTilesConfigBuilder = null;
            Profile profile = mTabModelSelector.getModel(/* incognito= */ false).getProfile();
            assumeNonNull(profile);
            CollaborationService collaborationService =
                    CollaborationServiceFactory.getForProfile(profile);
            ServiceStatus serviceStatus = collaborationService.getServiceStatus();
            if (serviceStatus.isAllowedToJoin()) {
                DataSharingService dataSharingService =
                        DataSharingServiceFactory.getForProfile(profile);
                sharedImageTilesConfigBuilder =
                        SharedImageTilesConfig.Builder.createForButton(mActivity)
                                .setIconSizeDp(R.dimen.tab_strip_shared_image_tiles_size);
                sharedImageTilesCoordinator =
                        new SharedImageTilesCoordinator(
                                mActivity,
                                sharedImageTilesConfigBuilder.build(),
                                dataSharingService,
                                collaborationService);
                FrameLayout container =
                        mToolbarView.findViewById(R.id.toolbar_image_tiles_container);
                TabUiUtils.attachSharedImageTilesCoordinatorToFrameLayout(
                        sharedImageTilesCoordinator, container);
            }

            mMediator =
                    new TabGroupUiMediator(
                            visibilityController,
                            mHandleBackPressChangedSupplier,
                            /* resetHandler= */ this,
                            mModel,
                            mTabModelSelector,
                            mTabContentManager,
                            mTabCreatorManager,
                            mLayoutStateProviderSupplier,
                            mTabGridDialogControllerSupplier,
                            mOmniboxFocusStateSupplier,
                            sharedImageTilesCoordinator,
                            sharedImageTilesConfigBuilder,
                            mThemeColorProvider,
                            onSnapshotTokenChange,
                            tabStripTokenSupplier);

            if (serviceStatus.isAllowedToJoin()) {
                mTabBubbler =
                        new TabBubbler(
                                profile,
                                mTabStripCoordinator.getTabListNotificationHandler(),
                                mCurrentTabGroupId);
            }
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
     * @param tabs List of Tabs to reset or null to clear.
     */
    @Override
    public void resetStripWithListOfTabs(@Nullable List<Tab> tabs) {
        assumeNonNull(mTabStripCoordinator);
        mTabStripCoordinator.resetWithListOfTabs(
                tabs, /* tabGroupSyncIds= */ null, /* quickMode= */ false);

        mCurrentTabGroupId.set(tabs == null || tabs.isEmpty() ? null : tabs.get(0).getTabGroupId());
        if (mTabBubbler != null) {
            mTabBubbler.showAll();
        }
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator} when the bottom sheet is
     * expanded or the dialog is shown.
     *
     * @param tabs List of Tabs to reset or null to clear.
     */
    @Override
    public void resetGridWithListOfTabs(@Nullable List<Tab> tabs) {
        if (mTabGridDialogControllerSupplier != null) {
            DialogController controller = mTabGridDialogControllerSupplier.get();
            assumeNonNull(controller);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                    && ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                    && ChromeFeatureList.isEnabled(
                            SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
                TabUiUtils.updateViewContentSensitivityForTabs(
                        tabs,
                        controller::setGridContentSensitivity,
                        "SensitiveContent.TabSwitching.BottomTabStripGroupUI.Sensitivity");
            }
            controller.resetWithListOfTabs(tabs);
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

    @Override
    public void destroy() {
        if (mTabStripCoordinator != null) {
            mTabStripCoordinator.onDestroy();
        }
        if (mSingleChildViewManager != null) {
            mSingleChildViewManager.destroy();
        }
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        if (mModelChangeProcessor != null) {
            mModelChangeProcessor.destroy();
        }
        if (mMediator != null) {
            mMediator.destroy();
        }
        if (mTabBubbler != null) {
            mTabBubbler.destroy();
        }
    }
}
