// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupCreationDialogManager.TabGroupCreationDialogManagerFactory;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Handles the flow of creating a new tab group through the UI. */
@NullMarked
public class TabGroupCreationUiDelegate {
    private final Context mContext;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<PaneManager> mPaneManagerSupplier;
    private final Supplier<@Nullable TabGroupModelFilter> mFilterSupplier;
    private final TabGroupCreationDialogManagerFactory mFactory;

    /**
     * @param context The context for this UI flow.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param paneManagerSupplier Supplies the {@link PaneManager}.
     * @param filterSupplier Supplies the filter used to create tab groups.
     * @param factory Used to create an instance of {@link TabGroupCreationDialogManager}
     */
    public TabGroupCreationUiDelegate(
            Context context,
            Supplier<ModalDialogManager> modalDialogManagerSupplier,
            Supplier<PaneManager> paneManagerSupplier,
            Supplier<@Nullable TabGroupModelFilter> filterSupplier,
            TabGroupCreationDialogManagerFactory factory) {
        mContext = context;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mPaneManagerSupplier = paneManagerSupplier;
        mFilterSupplier = filterSupplier;
        mFactory = factory;
    }

    /**
     * Creates a new tab group containing a newly created tab and starts a UI flow. This assumes
     * that the Hub is currently visible.
     *
     * <p>The UI flow is as follows:
     *
     * <ul>
     *   <li>A new tab is created in the background.
     *   <li>A tab group is created containing the new tab.
     *   <li>If successful, a {@link TabGroupCreationDialogManager} is shown to name the group.
     *   <li>On completion of the dialog, the Hub is focused on the Tab Switcher pane.
     *   <li>The Tab Switcher pane then opens the tab group UI for the newly created group.
     * </ul>
     */
    public void newTabGroupFlow() {
        TabGroupModelFilter filter = mFilterSupplier.get();
        assumeNonNull(filter);
        TabCreator tabCreator = filter.getTabModel().getTabCreator();

        Profile profile = filter.getTabModel().getProfile();
        UrlConstantResolver urlConstantResolver = UrlConstantResolverFactory.getForProfile(profile);

        @Nullable Tab tab =
                tabCreator.createNewTab(
                        new LoadUrlParams(urlConstantResolver.getNtpUrl()),
                        TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        null);
        if (tab != null) {
            filter.createSingleTabGroup(tab);
            @Nullable ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
            @Nullable Token tabGroupId = tab.getTabGroupId();
            if (tabGroupId != null && modalDialogManager != null) {
                mFactory.create(mContext, modalDialogManager, () -> openTabGroupUi(tab))
                        .showDialog(tabGroupId, filter);
            }
        }
    }

    private void openTabGroupUi(Tab tab) {
        @Nullable PaneManager paneManager = mPaneManagerSupplier.get();
        @Nullable Token groupId = tab.getTabGroupId();

        TabModel tabModel = assumeNonNull(mFilterSupplier.get()).getTabModel();
        @PaneId
        int tabSwitcher =
                tabModel.isIncognitoBranded() ? PaneId.INCOGNITO_TAB_SWITCHER : PaneId.TAB_SWITCHER;

        if (paneManager != null && groupId != null && paneManager.focusPane(tabSwitcher)) {
            @Nullable TabSwitcherPaneBase tabSwitcherPaneBase =
                    (TabSwitcherPaneBase) paneManager.getPaneForId(tabSwitcher);
            if (tabSwitcherPaneBase != null) {
                tabSwitcherPaneBase.requestOpenTabGroupDialog(tab.getId());
            }
        }
    }
}
