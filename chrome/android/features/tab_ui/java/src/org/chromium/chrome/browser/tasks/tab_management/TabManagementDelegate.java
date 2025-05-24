// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.util.Pair;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
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
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabModelDotInfo;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo.ArchivedTabsAutoDeletePromoManager;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;

/** Interface to get access to components concerning tab management. */
@NullMarked
public interface TabManagementDelegate {
    /**
     * Create the {@link TabGroupUi}.
     *
     * @param activity The {@link Activity} that creates this surface.
     * @param parentView The parent view of this UI.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} of the top
     *     controls.
     * @param scrimManager The {@link ScrimManager} to control scrim view.
     * @param omniboxFocusStateSupplier Supplier to access the focus state of the omnibox.
     * @param bottomSheetController The {@link BottomSheetController} for the current activity.
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
     *     UI and DataSharing services.
     * @param tabModelSelector Gives access to the current set of {@TabModel}.
     * @param tabContentManager Gives access to the tab content.
     * @param tabCreatorManager Manages creation of tabs.
     * @param layoutStateProviderSupplier Supplies the {@link LayoutStateProvider}.
     * @param modalDialogManager Used to show confirmation dialogs.
     * @param themeColorProvider Used to provide the theme.
     * @param undoBarThrottle Used to suppress the undo bar.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate} that will be used to share
     *     the tab's URL when the user selects the "Share" option.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @return The {@link TabGroupUi}.
     */
    TabGroupUi createTabGroupUi(
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
            Supplier<ShareDelegate> shareDelegateSupplier);

    /**
     * Create a {@link TabSwitcher} and {@link Pane} for the Hub.
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
     * @param dataSharingTabManager The {@link} DataSharingTabManager managing communication between
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
     * @param tabGroupCreationUiDelegate Orchestrates the tab group creation UI flow.
     * @param undoBarThrottle The controller to throttle the undo bar.
     * @param hubManagerSupplier Supplier ultimately used to get the pane manager to switch panes.
     * @param archivedTabsAutoDeletePromoManager Manager class for Archived Tabs Auto Delete Promo.
     * @param tabGroupUiActionHandlerSupplier Supplier for the controller used to open hidden
     *     groups.
     */
    Pair<TabSwitcher, Pane> createTabSwitcherPane(
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
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<TabModelDotInfo> tabModelNotificationDotSupplier,
            ObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            TabGroupCreationUiDelegate tabGroupCreationUiDelegate,
            UndoBarThrottle undoBarThrottle,
            LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @Nullable ArchivedTabsAutoDeletePromoManager archivedTabsAutoDeletePromoManager,
            Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier);

    /**
     * Create a {@link TabGroupsPane} for the Hub.
     *
     * @param context Used to inflate UI.
     * @param tabModelSelector Used to pull tab data from.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param profileProviderSupplier The supplier for profiles.
     * @param hubManagerSupplier Supplier ultimately used to get the pane manager to switch panes.
     * @param tabGroupUiActionHandlerSupplier Supplier for the controller used to open hidden
     *     groups.
     * @param modalDialogManagerSupplier Used to show confirmation dialogs.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param dataSharingTabManager The {@link} DataSharingTabManager to start collaboration flows.
     * @return The pane implementation that displays and allows interactions with tab groups.
     */
    Pane createTabGroupsPane(
            Context context,
            TabModelSelector tabModelSelector,
            DoubleConsumer onToolbarAlphaChange,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            LazyOneshotSupplier<HubManager> hubManagerSupplier,
            Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            DataSharingTabManager dataSharingTabManager);

    /**
     * Create a {@link TabGroupCreationUiDelegate} for tab group creation UI flows.
     *
     * @param context The {@link Context} for this UI flow.
     * @param modalDialogManager The modal dialog manager for the activity.
     * @param hubManagerSupplier Supplier ultimately used to get the pane manager to switch panes.
     * @param tabGroupModelFilterSupplier Supplies the current tab group model filter.
     */
    TabGroupCreationUiDelegate createTabGroupCreationUiFlow(
            Context context,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<HubManager> hubManagerSupplier,
            Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier);
}
