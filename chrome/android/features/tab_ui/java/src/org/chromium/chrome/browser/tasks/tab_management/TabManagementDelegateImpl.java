// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.util.Pair;
import android.view.View.OnClickListener;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.IncognitoStateProvider;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;

/** Impl class that will resolve components for tab management. */
public class TabManagementDelegateImpl implements TabManagementDelegate {
    @Override
    public TabGroupUi createTabGroupUi(
            @NonNull Activity activity,
            @NonNull ViewGroup parentView,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull IncognitoStateProvider incognitoStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull DataSharingTabManager dataSharingTabManager,
            TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @NonNull ModalDialogManager modalDialogManager) {
        return new TabGroupUiCoordinator(
                activity,
                parentView,
                browserControlsStateProvider,
                incognitoStateProvider,
                scrimCoordinator,
                omniboxFocusStateSupplier,
                bottomSheetController,
                dataSharingTabManager,
                tabModelSelector,
                tabContentManager,
                tabCreatorManager,
                layoutStateProviderSupplier,
                modalDialogManager);
    }

    @Override
    public Pair<TabSwitcher, Pane> createTabSwitcherPane(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull ScrimCoordinator rootUiScrimCoordinator,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull DataSharingTabManager dataSharingTabManager,
            @Nullable OneshotSupplier<IncognitoReauthController> incognitoReauthControllerSupplier,
            @NonNull OnClickListener newTabButtonOnClickListener,
            boolean isIncognito,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull BackPressManager backPressManager,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
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
                        rootUiScrimCoordinator,
                        snackbarManager,
                        modalDialogManager,
                        bottomSheetController,
                        dataSharingTabManager,
                        backPressManager,
                        desktopWindowStateProvider);
        OneshotSupplierImpl<Profile> profileSupplier = new OneshotSupplierImpl<>();
        Handler handler = new Handler();
        profileProviderSupplier.onAvailable(
                (profileProvider) -> profileSupplier.set(profileProvider.getOriginalProfile()));
        UserEducationHelper userEducationHelper =
                new UserEducationHelper(activity, profileSupplier, handler);

        TabSwitcherPaneBase pane;
        if (isIncognito) {
            Supplier<TabModelFilter> incongitorTabModelFilterSupplier =
                    () -> tabModelSelector.getTabModelFilterProvider().getTabModelFilter(true);
            pane =
                    new IncognitoTabSwitcherPane(
                            activity,
                            profileProviderSupplier,
                            factory,
                            incongitorTabModelFilterSupplier,
                            newTabButtonOnClickListener,
                            incognitoReauthControllerSupplier,
                            onToolbarAlphaChange,
                            userEducationHelper,
                            edgeToEdgeSupplier);
        } else {
            Supplier<TabModelFilter> tabModelFilterSupplier =
                    () -> tabModelSelector.getTabModelFilterProvider().getTabModelFilter(false);
            pane =
                    new TabSwitcherPane(
                            activity,
                            ContextUtils.getAppSharedPreferences(),
                            profileProviderSupplier,
                            factory,
                            tabModelFilterSupplier,
                            newTabButtonOnClickListener,
                            new TabSwitcherPaneDrawableCoordinator(activity, tabModelSelector),
                            onToolbarAlphaChange,
                            userEducationHelper,
                            edgeToEdgeSupplier);
        }
        return Pair.create(pane, pane);
    }

    @Override
    public Pane createTabGroupsPane(
            @NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull DoubleConsumer onToolbarAlphaChange,
            @NonNull OneshotSupplier<ProfileProvider> profileProviderSupplier,
            @NonNull LazyOneshotSupplier<HubManager> hubManagerSupplier,
            @NonNull Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier) {
        LazyOneshotSupplier<TabModelFilter> tabModelFilterSupplier =
                LazyOneshotSupplier.fromSupplier(
                        () ->
                                tabModelSelector
                                        .getTabModelFilterProvider()
                                        .getTabModelFilter(false));
        return new TabGroupsPane(
                context,
                tabModelFilterSupplier,
                onToolbarAlphaChange,
                profileProviderSupplier,
                () -> hubManagerSupplier.get().getPaneManager(),
                tabGroupUiActionHandlerSupplier,
                modalDialogManagerSupplier,
                edgeToEdgeSupplier);
    }
}
