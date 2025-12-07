// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayingAtLeast;

import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.addBlankTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;

import android.view.View;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;

import org.hamcrest.Matcher;
import org.mockito.Mock;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.test.R;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Objects;
import java.util.UUID;

/** Common utility methods for collaboration tests. */
public class CollaborationTestUtils {

    private static final long DELAY_MS_FOR_TAB_GROUP_ADDED = 1000;

    private final SyncTestRule mSyncTestRule;
    private final Profile mProfile;

    // Mock ShareDelegate and its supplier for controlling share sheet behavior.
    @Mock private final ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private final ShareDelegate mShareDelegate;

    public CollaborationTestUtils(SyncTestRule syncTestRule, Profile profile) {
        mSyncTestRule = syncTestRule;
        mProfile = profile;
        mShareDelegateSupplier = Mockito.mock(ObservableSupplier.class);
        mShareDelegate = Mockito.mock(ShareDelegate.class);
    }

    /**
     * Returns a {@link ViewAction} that performs a click with a relaxed display constraint. This is
     * useful for clicking views that might not be fully displayed (e.g., due to scrolling).
     */
    public static ViewAction relaxedClick() {
        final ViewAction clickAction = click();
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return isDisplayingAtLeast(51);
            }

            @Override
            public String getDescription() {
                return clickAction.getDescription();
            }

            @Override
            public void perform(UiController uiController, View view) {
                clickAction.perform(uiController, view);
            }
        };
    }

    /**
     * Checks if a dialog is fully visible within the given ChromeTabbedActivity.
     *
     * @param cta The ChromeTabbedActivity instance.
     * @return True if the dialog is fully visible, false otherwise.
     */
    public static boolean isDialogFullyVisible(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        View dialogContainerView = cta.findViewById(R.id.dialog_container_view);
        return dialogView.getVisibility() == View.VISIBLE && dialogContainerView.getAlpha() == 1f;
    }

    /**
     * Prepares to share the group with {@code tabGroupId} to the {@code collaborationId}. This is
     * updating the fake sync server.
     */
    public void prepareToShareGroup(LocalTabGroupId tabGroupId, String collaborationId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabGroupSyncService tabGroupSyncService = getTabGroupSyncService();
                    assert tabGroupSyncService != null && collaborationId != null;
                    tabGroupSyncService.setCollaborationAvailableInFinderForTesting(
                            collaborationId);
                    SavedTabGroup savedGroup = tabGroupSyncService.getGroup(tabGroupId);
                    assert savedGroup != null && savedGroup.collaborationId == null;
                    mSyncTestRule
                            .getFakeServerHelper()
                            .addCollaborationGroupToFakeServer(collaborationId);

                    mSyncTestRule.getSyncService().triggerRefresh();
                });
    }

    /**
     * Sets the {@code collaborationId} for the given {@code tabGroupId} and makes tab group shared.
     */
    public void makeTabGroupShared(LocalTabGroupId tabGroupId, String collaborationId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabGroupSyncService tabGroupSyncService = getTabGroupSyncService();
                    assert tabGroupSyncService != null && collaborationId != null;
                    prepareToShareGroup(tabGroupId, collaborationId);
                    SavedTabGroup savedGroup = tabGroupSyncService.getGroup(tabGroupId);
                    assert savedGroup != null && savedGroup.collaborationId == null;
                    tabGroupSyncService.makeTabGroupShared(
                            tabGroupId,
                            collaborationId,
                            (result) -> {
                                assert result;
                            });
                });
    }

    /**
     * Sets the {@code collaborationId} for the given {@code syncGroupId} and makes tab group
     * shared.
     */
    public void makeTabGroupShared(String syncGroupId, String collaborationId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabGroupSyncService tabGroupSyncService = getTabGroupSyncService();
                    assert tabGroupSyncService != null && collaborationId != null;
                    SavedTabGroup savedGroup = tabGroupSyncService.getGroup(syncGroupId);
                    assert savedGroup != null && savedGroup.collaborationId == null;
                    makeTabGroupShared(savedGroup.localId, collaborationId);
                });
    }

    /**
     * Creates the shared tab group with the given {@code collab_id} and saves it to the fake
     * server.
     */
    public void createSharedTabGroupInFakeServer(String collaborationId, String groupTitle) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabGroupSyncService tabGroupSyncService = getTabGroupSyncService();
                    assert tabGroupSyncService != null && collaborationId != null;
                    String syncGroupId = UUID.randomUUID().toString();
                    mSyncTestRule
                            .getFakeServerHelper()
                            .addSavedTabGroupToFakeServer(
                                    syncGroupId, groupTitle, /* numberOfTabs= */ 1);
                    mSyncTestRule.getSyncService().triggerRefresh();
                    // Post delayed task in order to make sure that `NotifyTabGroupAdded` is
                    // called first.
                    ThreadUtils.postOnUiThreadDelayed(
                            () -> {
                                makeTabGroupShared(syncGroupId, collaborationId);
                            },
                            DELAY_MS_FOR_TAB_GROUP_ADDED);
                });
    }

    /** Returns the {@link TabGroupSyncService} for the current profile. */
    public TabGroupSyncService getTabGroupSyncService() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return TabGroupSyncServiceFactory.getForProfile(mProfile);
                });
    }

    /** Signs in and sets selected types for tab groups. */
    public void setUpSyncAndSignIn() {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        mSyncTestRule.setSelectedTypes(
                true,
                new HashSet<>(
                        Arrays.asList(
                                UserSelectableType.TABS, UserSelectableType.SAVED_TAB_GROUPS)));
    }

    /** Returns the local tab group id. */
    public LocalTabGroupId getLocalTabGroupId(ChromeTabbedActivity cta) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return new LocalTabGroupId(
                            cta.getTabModelSelector().getModel(false).getTabAt(0).getTabGroupId());
                });
    }

    /** Creates a tab group and opens the tab grid dialog. */
    public void createTabGroupAndOpenTabGridDialog(ChromeTabbedActivity cta) {
        addBlankTabs(cta, false, 2);
        enterTabSwitcher(cta);
        mergeAllNormalTabsToAGroup(cta);
        clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> CollaborationTestUtils.isDialogFullyVisible(cta));
    }

    /** Mocks share delegate and returns success for opening the share sheet. */
    public void setupShareDelegateSupplier(ChromeTabbedActivity cta) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    doReturn(mShareDelegate).when(mShareDelegateSupplier).get();
                    var rootUiCoordinator = cta.getRootUiCoordinatorForTesting();
                    DataSharingTabManager dstm = rootUiCoordinator.getDataSharingTabManager();
                    dstm.setShareDelegateSupplierForTesting(mShareDelegateSupplier);
                });
    }

    /** Returns whether the shared tab group still exists in the tab group sync service. */
    public boolean collaborationExistsInSyncService(String collaborationId) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabGroupSyncService tabGroupSyncService = getTabGroupSyncService();
                    for (String syncId : tabGroupSyncService.getAllGroupIds()) {
                        SavedTabGroup group = tabGroupSyncService.getGroup(syncId);
                        if (group == null) continue;
                        if (Objects.equals(group.collaborationId, collaborationId)) return true;
                    }
                    return false;
                });
    }

    /** Converts an {@link AccountInfo} to a {@link GroupMember}. */
    public static GroupMember accountInfoToGroupMember(AccountInfo info, @MemberRole int role) {
        return new GroupMember(
                info.getGaiaId(),
                info.getFullName(),
                info.getEmail(),
                role,
                /* avatarUrl= */ GURL.emptyGURL(),
                info.getGivenName());
    }
}
