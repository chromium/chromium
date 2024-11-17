// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.content.Context;
import android.content.Intent;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.hub.HubManager;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab_ui.TabSwitcherCustomViewManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** A factory to create an {@link IncognitoReauthCoordinator} instance. */
public class IncognitoReauthCoordinatorFactory {
    /** The context to use in order to inflate the re-auth view and fetch the resources. */
    private final @NonNull Context mContext;

    /**
     * The {@link TabModelSelector} instance used for providing functionality to toggle between tab
     * models and to close Incognito tabs.
     */
    private final @NonNull TabModelSelector mTabModelSelector;

    /** The manager responsible for showing the full screen Incognito re-auth dialog. */
    private final @NonNull ModalDialogManager mModalDialogManager;

    /** The manager responsible for invoking the underlying native re-authentication. */
    private final @NonNull IncognitoReauthManager mIncognitoReauthManager;

    /**
     * A boolean to distinguish between tabbedActivity or CCT, during coordinator creation.
     *
     * <p>TODO(crbug.com/40056462): Remove the need for this and instead populate the various {@link
     * Runnable} that we create here at the client site directly.
     */
    private final boolean mIsTabbedActivity;

    /**
     * This allows to pass the re-auth view to the tab switcher. Non-null contents for {@link
     * TabSwitcherIncognitoReauthCoordinator}.
     */
    private final OneshotSupplierImpl<TabSwitcherCustomViewManager>
            mTabSwitcherCustomViewManagerSupplier = new OneshotSupplierImpl<>();

    /**
     * This allows to show the regular overview mode.
     * Non-null for {@link FullScreenIncognitoReauthCoordinator}.
     */
    private @Nullable LayoutManager mLayoutManager;

    /** Supplier for the HubManager. Non-null for tabbed mode. */
    private final @Nullable OneshotSupplier<HubManager> mHubManagerSupplier;

    /** An {@link Intent} which allows to opens regular overview mode from a non-tabbed Activity. */
    private @Nullable Intent mShowRegularOverviewIntent;

    /** A test-only variable used to mock the menu delegate instead of creating one. */
    @VisibleForTesting IncognitoReauthMenuDelegate mIncognitoReauthMenuDelegateForTesting;

    /**
     * @param context The {@link Context} to use for fetching the re-auth resources.
     * @param tabModelSelector The {@link TabModelSelector} to use for toggling between regular and
     *     incognito model and to close all Incognito tabs.
     * @param modalDialogManager The {@link ModalDialogManager} to use for firing the dialog
     *     containing the Incognito re-auth view.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} instance which would be used
     *     to initiate re-authentication.
     * @param layoutManager {@link LayoutManager} to use for showing the regular overview mode.
     * @param hubManagerSupplier The supplier of the {@link HubManager}.
     * @param showRegularOverviewIntent An {@link Intent} to show the regular overview mode.
     * @param isTabbedActivity A boolean to indicate if the re-auth screen being fired from
     */
    public IncognitoReauthCoordinatorFactory(
            @NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull IncognitoReauthManager incognitoReauthManager,
            @Nullable LayoutManager layoutManager,
            @Nullable OneshotSupplier<HubManager> hubManagerSupplier,
            @Nullable Intent showRegularOverviewIntent,
            boolean isTabbedActivity) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mModalDialogManager = modalDialogManager;
        mIncognitoReauthManager = incognitoReauthManager;
        mLayoutManager = layoutManager;
        mHubManagerSupplier = hubManagerSupplier;
        mShowRegularOverviewIntent = showRegularOverviewIntent;
        mIsTabbedActivity = isTabbedActivity;

        assert isTabbedActivity || mShowRegularOverviewIntent != null
                : "A valid intent is required to be able to"
                        + " open regular overview mode from inside non-tabbed Activity.";
    }

    /**
     * @param tabSwitcherCustomViewManager {@link TabSwitcherCustomViewManager} to use for
     *         communicating with tab switcher to show the re-auth screen.
     */
    public void setTabSwitcherCustomViewManager(
            @NonNull TabSwitcherCustomViewManager tabSwitcherCustomViewManager) {
        mTabSwitcherCustomViewManagerSupplier.set(tabSwitcherCustomViewManager);
    }

    /**
     * Returns the {@link OneshotSupplier<TabSwitcherCustomViewManager>} that is used to pass the
     * re-auth screen to tab switcher.
     */
    public OneshotSupplier<TabSwitcherCustomViewManager> getTabSwitcherCustomViewManagerSupplier() {
        return mTabSwitcherCustomViewManagerSupplier;
    }

    /**
     * This method is responsible for clean-up work. Typically, called when the Activity is being
     * destroyed.
     */
    void destroy() {
        mIncognitoReauthManager.destroy();
    }

    private IncognitoReauthMenuDelegate getIncognitoReauthMenuDelegate() {
        if (mIncognitoReauthMenuDelegateForTesting != null) {
            return mIncognitoReauthMenuDelegateForTesting;
        }

        return new IncognitoReauthMenuDelegate(mContext, getCloseAllIncognitoTabsRunnable());
    }

    /**
     * @return {@link Runnable} to use when the user click on "See other tabs".
     */
    @VisibleForTesting
    Runnable getSeeOtherTabsRunnable() {
        if (mIsTabbedActivity) {
            return () -> {
                mTabModelSelector.selectModel(/* incognito= */ false);
                if (mLayoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                    mHubManagerSupplier.get().getPaneManager().focusPane(PaneId.TAB_SWITCHER);
                    return;
                }
                mLayoutManager.showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
            };
        } else {
            return () -> mContext.startActivity(mShowRegularOverviewIntent);
        }
    }

    /**
     * @return {@link Runnable} to use when the user clicks on "Close all Incognito tabs" option.
     */
    @VisibleForTesting
    Runnable getCloseAllIncognitoTabsRunnable() {
        return IncognitoTabHostUtils::closeAllIncognitoTabs;
    }

    /**
     * @return {@link Runnable} to use when the user presses back while the re-auth is being shown.
     */
    Runnable getBackPressRunnable() {
        // Both "See other tabs" and back-press shares the same logic.
        return getSeeOtherTabsRunnable();
    }

    /**
     * @return True, if the re-auth is being triggered from a tabbed Activity, false otherwise.
     */
    boolean getIsTabbedActivity() {
        return mIsTabbedActivity;
    }

    /**
     * @param showFullScreen The type of dependency to check for.
     * @return whether the factory has all the dependencies ready to produce the requested type of
     *     coordinator.
     */
    boolean areDependenciesReadyFor(boolean showFullScreen) {
        return showFullScreen || mTabSwitcherCustomViewManagerSupplier.hasValue();
    }

    /**
     * @param incognitoReauthCallback The {@link IncognitoReauthCallback}
     *                               which would be executed after an authentication attempt.
     * @param showFullScreen A boolean indicating whether to show a fullscreen or tab switcher
     *                      Incognito reauth.
     *
     * @param backPressedCallback {@link OnBackPressedCallback} which would be called when a user
     *         presses back while the fullscreen re-auth is shown.
     *
     * @return {@link IncognitoReauthCoordinator} instance spliced on fullscreen/tab-switcher and
     * tabbedActivity/CCT.
     */
    IncognitoReauthCoordinator createIncognitoReauthCoordinator(
            @NonNull IncognitoReauthCallback incognitoReauthCallback,
            boolean showFullScreen,
            @NonNull OnBackPressedCallback backPressedCallback) {
        assert areDependenciesReadyFor(showFullScreen)
                : "Dependencies for the IncognitoReauthCoordinator were not ready.";
        return showFullScreen
                ? new FullScreenIncognitoReauthCoordinator(
                        mContext,
                        mIncognitoReauthManager,
                        incognitoReauthCallback,
                        getSeeOtherTabsRunnable(),
                        mModalDialogManager,
                        getIncognitoReauthMenuDelegate(),
                        backPressedCallback)
                : new TabSwitcherIncognitoReauthCoordinator(
                        mContext,
                        mIncognitoReauthManager,
                        incognitoReauthCallback,
                        getSeeOtherTabsRunnable(),
                        getBackPressRunnable(),
                        mTabSwitcherCustomViewManagerSupplier.get());
    }
}
