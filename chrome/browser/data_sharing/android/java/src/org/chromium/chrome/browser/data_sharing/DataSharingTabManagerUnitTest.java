// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.components.data_sharing.SharedGroupTestHelper.ACCESS_TOKEN1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.tab_group_sync.SyncedGroupTestHelper.SYNC_GROUP_ID1;

import android.app.Activity;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.collaboration.CollaborationControllerDelegateFactory;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.collaboration.CollaborationControllerDelegate;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.messaging.ActivityLogItem;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.ParseUrlResult;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.ParseUrlStatus;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SyncedGroupTestHelper;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link DataSharingTabManager} */
@RunWith(BaseRobolectricTestRunner.class)
public class DataSharingTabManagerUnitTest {
    private static final LocalTabGroupId LOCAL_ID = new LocalTabGroupId(Token.createRandom());
    private static final Integer TAB_ID = 456;
    private static final int TAB_GROUP_ROOT_ID = 148;
    private static final String TITLE = "Title";
    private static final GURL TEST_URL = JUnitTestGURLs.URL_1;

    private static final class JoinTestHelper {
        /* package */ final Callback<Boolean> mOnJoinFinished = this::onJoinFinished;

        private boolean mWasJoinFinished;

        /* package */ JoinTestHelper() {}

        private void onJoinFinished(boolean success) {
            assertTrue(success);
            mWasJoinFinished = true;
        }
    }

    private final OneshotSupplierImpl<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier =
            new OneshotSupplierImpl<>();
    private final JoinTestHelper mJoinTestHelper = new JoinTestHelper();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private CollaborationControllerDelegate mCollaborationControllerDelegate;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private DataSharingTabGroupsDelegate mDataSharingTabGroupsDelegate;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;
    @Mock private Callback<Boolean> mCreateGroupFinishedCallback;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupUiActionHandler mTabGroupUiActionHandler;
    @Mock private TabModel mTabModel;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private Tab mTab;

    @Captor private ArgumentCaptor<Callback<Integer>> mOutcomeCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mTabGroupPreviewCallbackCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;
    @Captor private ArgumentCaptor<ShareParams> mShareParamsCaptor;

    private DataSharingTabManager mDataSharingTabManager;
    private SavedTabGroup mSavedTabGroup;
    private Activity mActivity;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    private Supplier<BottomSheetController> mBottomSheetControllerSupplier;
    private ObservableSupplier<ShareDelegate> mShareDelegateSupplier;
    private SyncedGroupTestHelper mSyncedGroupTestHelper;

    @Before
    public void setUp() {
        DomDistillerUrlUtilsJni.setInstanceForTesting(mDistillerUrlUtilsJniMock);

        DataSharingServiceFactory.setForTesting(mDataSharingService);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>(mTabModelSelector);
        mBottomSheetControllerSupplier = new ObservableSupplierImpl<>(mBottomSheetController);
        mShareDelegateSupplier = new ObservableSupplierImpl<>(mShareDelegate);
        mTabGroupUiActionHandlerSupplier.set(mTabGroupUiActionHandler);

        CollaborationControllerDelegateFactory collaborationControllerDelegateFactory =
                (type, runnable) -> {
                    return mCollaborationControllerDelegate;
                };

        mDataSharingTabManager =
                new DataSharingTabManager(
                        mTabModelSelectorSupplier,
                        mDataSharingTabGroupsDelegate,
                        mBottomSheetControllerSupplier,
                        mShareDelegateSupplier,
                        mWindowAndroid,
                        ApplicationProvider.getApplicationContext().getResources(),
                        mTabGroupUiActionHandlerSupplier,
                        collaborationControllerDelegateFactory);

        mDataSharingTabManager.initWithProfile(
                mProfile, mDataSharingService, mMessagingBackendService, mCollaborationService);

        mSyncedGroupTestHelper = new SyncedGroupTestHelper(mTabGroupSyncService);
        mSavedTabGroup = mSyncedGroupTestHelper.newTabGroup(SYNC_GROUP_ID1);
        mSavedTabGroup.collaborationId = COLLABORATION_ID1;
        mSavedTabGroup.localId = LOCAL_ID;
        mSavedTabGroup.savedTabs = SyncedGroupTestHelper.tabsFromIds(TAB_ID);
        mSavedTabGroup.savedTabs.get(0).url = new GURL("https://www.example.com/");

        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mWindowAndroid.getModalDialogManager()).thenReturn(mModalDialogManager);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));

        mActivityScenarioRule.getScenario().onActivity(this::onActivityCreated);
    }

    private void onActivityCreated(Activity activity) {
        mActivity = activity;
    }

    private void mockSuccessfulParseDataSharingUrl() {
        GroupToken groupToken = new GroupToken(COLLABORATION_ID1, ACCESS_TOKEN1);
        ParseUrlResult result =
                new DataSharingService.ParseUrlResult(groupToken, ParseUrlStatus.SUCCESS);
        when(mDataSharingService.parseDataSharingUrl(any())).thenReturn(result);
    }

    private void setupTabGroupFilterForOpenGroupWithId() {
        doReturn(mTabGroupModelFilterProvider)
                .when(mTabModelSelector)
                .getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getTabGroupModelFilter(anyBoolean());
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(SYNC_GROUP_ID1);
        doReturn(TAB_GROUP_ROOT_ID).when(mTabGroupModelFilter).getRootIdFromTabGroupId(any());
    }

    @Test
    public void testNoProfile() {
        mDataSharingTabManager =
                new DataSharingTabManager(
                        mTabModelSelectorSupplier,
                        mDataSharingTabGroupsDelegate,
                        mBottomSheetControllerSupplier,
                        mShareDelegateSupplier,
                        mWindowAndroid,
                        ApplicationProvider.getApplicationContext().getResources(),
                        mTabGroupUiActionHandlerSupplier,
                        null);
        mDataSharingTabManager.initiateJoinFlow(TEST_URL);

        // Verify we never parse the URL without a profile.
        verify(mDataSharingService, never()).parseDataSharingUrl(TEST_URL);
    }

    @Test
    public void testJoinFlowWithCollaborationService() {
        mockSuccessfulParseDataSharingUrl();
        mDataSharingTabManager.initiateJoinFlow(TEST_URL);

        verify(mCollaborationService).startJoinFlow(any(), eq(TEST_URL), anyInt());
    }

    @Test
    public void testManageSharing() {
        mDataSharingTabManager.showManageSharing(
                mActivity, COLLABORATION_ID1, /* outcomeCallback= */ null);
    }

    @Test
    public void testDestroy() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mockSuccessfulParseDataSharingUrl();
        mDataSharingTabManager.initiateJoinFlow(TEST_URL);

        mDataSharingTabManager.destroy();
        verify(mFaviconHelper).destroy();
    }

    @Test
    public void testShareOrManageFlowWithCollaborationService() {
        doReturn(mProfile).when(mProfile).getOriginalProfile();
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);
        mDataSharingTabManager.createOrManageFlow(
                mActivity, /* syncId= */ null, LOCAL_ID, anyInt(), null);

        verify(mCollaborationService).startShareOrManageFlow(any(), eq(SYNC_GROUP_ID1), anyInt());
    }

    @Test
    public void testCreateTabGroupAndShare_withSingleTab() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        doReturn(mProfile).when(mProfile).getOriginalProfile();

        mTabModelSelectorSupplier.set(mTabModelSelector);
        doReturn(mTabGroupModelFilterProvider)
                .when(mTabModelSelector)
                .getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getTabGroupModelFilter(anyBoolean());
        when(mTab.getTabGroupId()).thenReturn(null).thenReturn(LOCAL_ID.tabGroupId);

        mSavedTabGroup.collaborationId = null;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createTabGroupAndShare(
                mActivity,
                mTab,
                CollaborationServiceShareOrManageEntryPoint.UNKNOWN,
                mCreateGroupFinishedCallback);

        verify(mTabGroupModelFilter).createSingleTabGroup(eq(mTab));
        verify(mCollaborationService).startShareOrManageFlow(any(), eq(SYNC_GROUP_ID1), anyInt());
    }

    @Test
    public void testCreateTabGroupAndShare_withExistingTabGroup() {
        mDataSharingTabManager
                .getBulkFaviconUtilForTesting()
                .setFaviconHelperForTesting(mFaviconHelper);
        doReturn(mProfile).when(mProfile).getOriginalProfile();

        mTabModelSelectorSupplier.set(mTabModelSelector);
        doReturn(mTabGroupModelFilterProvider)
                .when(mTabModelSelector)
                .getTabGroupModelFilterProvider();
        doReturn(mTabGroupModelFilter)
                .when(mTabGroupModelFilterProvider)
                .getTabGroupModelFilter(anyBoolean());
        doReturn(LOCAL_ID.tabGroupId).when(mTab).getTabGroupId();

        mSavedTabGroup.collaborationId = null;
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);

        mDataSharingTabManager.createTabGroupAndShare(
                mActivity,
                mTab,
                CollaborationServiceShareOrManageEntryPoint.UNKNOWN,
                mCreateGroupFinishedCallback);

        verify(mCollaborationService).startShareOrManageFlow(any(), eq(SYNC_GROUP_ID1), anyInt());
    }

    @Test
    public void testShowRecentActivity() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        // mDataSharingTabManager.setFaviconHelperForTesting(mFaviconHelper);
        doReturn(mSavedTabGroup).when(mTabGroupSyncService).getGroup(LOCAL_ID);
        setupActivityLogItemsOnTheBackend();
        mDataSharingTabManager.showRecentActivity(mActivity, COLLABORATION_ID1);
        verify(mMessagingBackendService).getActivityLog(any());
    }

    @Test
    public void testPromoteTabGroup() {
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        setupTabGroupFilterForOpenGroupWithId();

        mDataSharingTabManager.promoteTabGroup(COLLABORATION_ID1);
        verify(mDataSharingTabGroupsDelegate).openTabGroupWithTabId(TAB_GROUP_ROOT_ID);
    }

    private void setupActivityLogItemsOnTheBackend() {
        List<ActivityLogItem> logItems = new ArrayList<>();

        // Add item that has action to focus tab.
        ActivityLogItem logItem1 = new ActivityLogItem();
        logItem1.collaborationEvent = CollaborationEvent.TAB_ADDED;
        setTabMetadata(logItem1, mSavedTabGroup.savedTabs.get(0).localId, null);
        logItems.add(logItem1);

        // Add item that has action to reopen tab.
        ActivityLogItem logItem2 = new ActivityLogItem();
        logItem2.collaborationEvent = CollaborationEvent.TAB_REMOVED;
        setTabMetadata(logItem2, null, "https://google.com");
        logItems.add(logItem2);

        // Add item that has action to show tab group dialog.
        ActivityLogItem logItem3 = new ActivityLogItem();
        logItem3.collaborationEvent = CollaborationEvent.TAB_GROUP_COLOR_UPDATED;
        setTabGroupMetadata(logItem3, mSavedTabGroup.localId);
        setTabMetadata(logItem3, null, null);
        logItems.add(logItem3);

        // Add item that has action to show share UI.
        ActivityLogItem logItem4 = new ActivityLogItem();
        logItem4.collaborationEvent = CollaborationEvent.COLLABORATION_MEMBER_ADDED;
        setCollaborationId(logItem4, COLLABORATION_ID1);
        setTabMetadata(logItem4, null, null);
        logItems.add(logItem4);

        when(mMessagingBackendService.getActivityLog(any())).thenReturn(logItems);
    }

    private void setCollaborationId(ActivityLogItem logItem, String collaborationId) {
        if (logItem.activityMetadata == null) logItem.activityMetadata = new MessageAttribution();
        logItem.activityMetadata.collaborationId = collaborationId;
    }

    private void setTabGroupMetadata(ActivityLogItem logItem, LocalTabGroupId localTabGroupId) {
        if (logItem.activityMetadata == null) logItem.activityMetadata = new MessageAttribution();
        if (logItem.activityMetadata.tabGroupMetadata == null) {
            logItem.activityMetadata.tabGroupMetadata = new TabGroupMessageMetadata();
        }
        logItem.activityMetadata.tabGroupMetadata.localTabGroupId = localTabGroupId;
    }

    private void setTabMetadata(
            ActivityLogItem logItem, @Nullable Integer tabId, @Nullable String lastKnownUrl) {
        if (logItem.activityMetadata == null) logItem.activityMetadata = new MessageAttribution();
        if (logItem.activityMetadata.tabMetadata == null) {
            logItem.activityMetadata.tabMetadata = new TabMessageMetadata();
        }
        TabMessageMetadata tabMessageMetadata = logItem.activityMetadata.tabMetadata;
        if (tabId != null) {
            tabMessageMetadata.localTabId = tabId;
        }
        if (!TextUtils.isEmpty(lastKnownUrl)) {
            tabMessageMetadata.lastKnownUrl = lastKnownUrl;
        }
    }
}
