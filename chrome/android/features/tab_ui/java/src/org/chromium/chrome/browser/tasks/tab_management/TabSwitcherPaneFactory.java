// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.os.Handler;
import android.util.Pair;
import android.view.View.OnClickListener;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo.ArchivedTabsAutoDeletePromoManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;
import java.util.function.Supplier;

/** Factory for creating {@link TabSwitcher} and {@link Pane} instances for the Hub. */
@NullMarked
public class TabSwitcherPaneFactory {
    private TabSwitcherPaneFactory() {}

    /**
     * Creates a {@link TabSwitcher} and {@link Pane} for the Hub.
     *
     * @param activity The {@link Activity} that hosts the pane.
     * @param lifecycleDispatcher The lifecycle dispatcher for the activity.
     * @param profileProviderSupplier The supplier for profiles.
     * @param tabModelSelector For access to {@link TabModel}.
     * @param tabContentManager For management of thumbnails.
     * @param tabCreatorManager For creating new tabs.
     * @param browserControlsStateProvider For determining thumbnail size.
     * @param multiWindowModeStateDispatcher For managing behavior in multi-window.
     * @param scrimManager The root UI coordinator's scrim component. On LFF this is unused as the
     *     root UI's scrim component is used for the show/hide animation.
     * @param snackbarManager The activity level snackbar manager.
     * @param modalDialogManager The modal dialog manager for the activity.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param dataSharingTabManager The {@link DataSharingTabManager} managing communication between
     *     UI and DataSharing services.
     * @param incognitoReauthControllerSupplier The incognito reauth controller supplier.
     * @param newTabButtonOnClickListener The listener for clicking the new tab button.
     * @param isIncognito Whether this is an incognito pane.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param backPressManager Manages different back press handlers throughout the app.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param desktopWindowStateManager Manager to get desktop window and app header state.
     * @param tabModelNotificationDotSupplier Supplier for whether the notification dot should show
     *     on the tab switcher drawable.
     * @param compositorViewHolderSupplier Supplier to the {@link CompositorViewHolder} instance.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @param undoBarThrottle The controller to throttle the undo bar.
     * @param hubManagerSupplier Supplier ultimately used to get the pane manager to switch panes.
     * @param archivedTabsAutoDeletePromoManager Manager class for Archived Tabs Auto Delete Promo.
     * @param tabGroupUiActionHandlerSupplier Supplier for the controller used to open hidden
     *     groups.
     * @param layoutStateProviderSupplier Supplies the LayoutStateProvider, which is used to observe
     *     when the TabSwitcher is hidden.
     * @param xrSpaceModeObservableSupplier Supplies current XR space mode status. True for XR full
     *     space mode, false otherwise.
     * @param multiInstanceManager An instance of the {@link MultiInstanceManager}.
     * @param dragDropDelegate {@link DragAndDropDelegate} to initiate tab drag and drop.
     * @param dragHandlerManager Manages back press during tab switcher drag and drop.
     * @return A {@link Pair} of the created {@link TabSwitcher} and {@link Pane}.
     */
    public static Pair<TabSwitcher, Pane> createTabSwitcherPane(
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            TabCreatorManager tabCreatorManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ScrimManager scrimManager,
            SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager,
            BottomSheetController bottomSheetController,
            DataSharingTabManager dataSharingTabManager,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            OnClickListener newTabButtonOnClickListener,
            boolean isIncognito,
            DoubleConsumer onToolbarAlphaChange,
            BackPressManager backPressManager,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            MonotonicObservableSupplier<TabModelDotInfo> tabModelNotificationDotSupplier,
            MonotonicObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            MonotonicObservableSupplier<ShareDelegate> shareDelegateSupplier,
            MonotonicObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            UndoBarThrottle undoBarThrottle,
            LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @Nullable ArchivedTabsAutoDeletePromoManager archivedTabsAutoDeletePromoManager,
            Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            Supplier<LayoutStateProvider> layoutStateProviderSupplier,
            NonNullObservableSupplier<Boolean> xrSpaceModeObservableSupplier,
            @Nullable MultiInstanceManager multiInstanceManager,
            @Nullable DragAndDropDelegate dragDropDelegate,
            TabSwitcherBackPressHandlerManager dragHandlerManager) {
        TabGroupCreationUiDelegate tabGroupCreationUiDelegate =
                new TabGroupCreationUiDelegate(
                        activity,
                        () -> modalDialogManager,
                        () -> assumeNonNull(hubManagerSupplier.get()).getPaneManager(),
                        tabModelSelector.getCurrentTabModelSupplier(),
                        TabGroupCreationDialogManager::new);

        @Nullable TabSwitcherDragHandler tabSwitcherDragHandler = null;
        if (dragDropDelegate != null && multiInstanceManager != null) {
            tabSwitcherDragHandler =
                    new TabSwitcherDragHandler(
                            () -> activity,
                            multiInstanceManager,
                            dragDropDelegate,
                            dragHandlerManager,
                            /* fadeDragShadow= */ true);
            tabSwitcherDragHandler.setTabModelSelector(tabModelSelector);
            if (!backPressManager.has(BackPressHandler.Type.CANCEL_TAB_SWITCHER_DRAG)) {
                backPressManager.addHandler(
                        dragHandlerManager, BackPressHandler.Type.CANCEL_TAB_SWITCHER_DRAG);
            }
        }

        // TODO(crbug.com/40946413): Consider making this an activity scoped singleton and possibly
        // hosting it in CTA/HubProvider.
        TabSwitcherPaneCoordinatorFactory factory =
                new TabSwitcherPaneCoordinatorFactory(
                        activity,
                        lifecycleDispatcher,
                        profileProviderSupplier,
                        tabModelSelector,
                        tabContentManager,
                        tabCreatorManager,
                        browserControlsStateProvider,
                        multiWindowModeStateDispatcher,
                        scrimManager,
                        snackbarManager,
                        modalDialogManager,
                        bottomSheetController,
                        dataSharingTabManager,
                        backPressManager,
                        desktopWindowStateManager,
                        edgeToEdgeSupplier,
                        shareDelegateSupplier,
                        tabBookmarkerSupplier,
                        undoBarThrottle,
                        () -> assumeNonNull(hubManagerSupplier.get()).getPaneManager(),
                        tabGroupUiActionHandlerSupplier,
                        layoutStateProviderSupplier,
                        tabSwitcherDragHandler);
        OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
        Handler handler = new Handler();
        profileProviderSupplier.onAvailable(
                (profileProvider) -> profileSupplier.set(profileProvider.getOriginalProfile()));
        UserEducationHelper userEducationHelper =
                new UserEducationHelper(activity, profileSupplier, handler);

        Supplier<TabModel> tabModelSupplier = () -> tabModelSelector.getModel(isIncognito);
        TabSwitcherPaneBase pane =
                isIncognito
                        ? new IncognitoTabSwitcherPane(
                                activity,
                                factory,
                                tabModelSupplier,
                                newTabButtonOnClickListener,
                                incognitoReauthControllerSupplier,
                                onToolbarAlphaChange,
                                userEducationHelper,
                                edgeToEdgeSupplier,
                                compositorViewHolderSupplier,
                                tabGroupCreationUiDelegate,
                                xrSpaceModeObservableSupplier)
                        : new TabSwitcherPane(
                                activity,
                                ContextUtils.getAppSharedPreferences(),
                                profileProviderSupplier,
                                factory,
                                tabModelSupplier,
                                newTabButtonOnClickListener,
                                new TabSwitcherPaneDrawableCoordinator(
                                        activity,
                                        tabModelSelector,
                                        tabModelNotificationDotSupplier),
                                onToolbarAlphaChange,
                                userEducationHelper,
                                edgeToEdgeSupplier,
                                compositorViewHolderSupplier,
                                tabGroupCreationUiDelegate,
                                archivedTabsAutoDeletePromoManager,
                                xrSpaceModeObservableSupplier);
        return Pair.create(pane, pane);
    }
}
