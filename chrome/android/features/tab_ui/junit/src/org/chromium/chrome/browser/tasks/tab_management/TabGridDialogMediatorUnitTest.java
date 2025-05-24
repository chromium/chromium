// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.SUPPRESS_ACCESSIBILITY;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.COLLABORATION_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.EMAIL1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.EMAIL2;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GAIA_ID1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GAIA_ID2;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;

import android.app.Activity;
import android.graphics.Rect;
import android.os.SystemClock;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.MotionEvent;
import android.view.View;
import android.widget.EditText;

import androidx.annotation.IdRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabRemover;
import org.chromium.chrome.browser.tabmodel.TabUiUnitTestUtils;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabGridItemLongPressOrchestrator.CancelLongPressTabItemEventListener;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;
import org.chromium.components.browser_ui.widget.list_view.FakeListViewTouchTracker;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.collaboration.messaging.CollaborationEvent;
import org.chromium.components.collaboration.messaging.MessageAttribution;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;
import org.chromium.components.collaboration.messaging.TabGroupMessageMetadata;
import org.chromium.components.collaboration.messaging.TabMessageMetadata;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;

/** Tests for {@link TabGridDialogMediator}. */
@SuppressWarnings({"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored"})
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
@DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
public class TabGridDialogMediatorUnitTest {
    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String DIALOG_TITLE1 = "1 tab";
    private static final String DIALOG_TITLE2 = "2 tabs";
    private static final String CUSTOMIZED_DIALOG_TITLE = "Cool Tabs";
    private static final String GROUP_TITLE = "My Group";
    private static final int COLOR_2 = 1;
    private static final int COLOR_3 = 2;
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);
    private static final LocalTabGroupId LOCAL_TAB_GROUP_ID = new LocalTabGroupId(TAB_GROUP_ID);
    private static final EitherGroupId EITHER_LOCAL_TAB_GROUP_ID =
            EitherGroupId.createLocalId(LOCAL_TAB_GROUP_ID);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private View mView;
    @Mock private TabGridDialogMediator.DialogController mDialogController;
    @Mock private TabCreator mTabCreator;
    @Mock private TabSwitcherResetHandler mTabSwitcherResetHandler;
    @Mock private TabGridDialogMediator.AnimationSourceViewProvider mAnimationSourceViewProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabRemover mTabRemover;
    @Mock private TabListEditorCoordinator.TabListEditorController mTabListEditorController;
    @Mock private EditText mTitleTextView;
    @Mock private Editable mEditable;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Supplier<RecyclerViewPosition> mRecyclerViewPositionSupplier;
    @Mock private DataSharingTabManager mDataSharingTabManager;
    @Mock private Runnable mShowColorPickerPopupRunnable;
    @Mock private ActionConfirmationManager mActionConfirmationManager;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private DataSharingService mDataSharingService;
    @Mock private DataSharingUIDelegate mDataSharingUiDelegate;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    @Mock private DesktopWindowStateManager mDesktopWindowStateManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Tracker mTracker;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private ObservableSupplier<TabBookmarker> mTabBookmarkerSupplier;
    @Mock private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private View mCardView;
    @Mock private TabGridContextMenuCoordinator mTabGridContextMenuCoordinator;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mCollaborationActivityMessageCardCaptor;
    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mMessageCardModelCaptor;
    @Captor private ArgumentCaptor<DataSharingService.Observer> mDataSharingServiceObserverCaptor;

    @Captor
    private ArgumentCaptor<MessagingBackendService.PersistentMessageObserver>
            mPersistentMessageObserverCaptor;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private final ObservableSupplierImpl<TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private UserActionTester mActionTester;
    private Tab mTab1;
    private Tab mTab2;
    private List<Tab> mTabList;
    private Activity mActivity;
    private PropertyModel mModel;
    private TabGridDialogMediator mMediator;

    @Before
    public void setUp() {
        mActionTester = new UserActionTester();

        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        BookmarkModel.setInstanceForTesting(mBookmarkModel);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mProfile.isNativeInitialized()).thenReturn(true);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        when(mDataSharingService.getUiDelegate()).thenReturn(mDataSharingUiDelegate);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        when(mServiceStatus.isAllowedToJoin()).thenReturn(true);
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        mockPersistentMessages(/* added= */ 1, /* navigated= */ 2, /* removed= */ 4);
        TrackerFactory.setTrackerForTests(mTracker);

        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));
        mTabList = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        when(mTabGroupModelFilter.getRepresentativeTabList()).thenReturn(mTabList);

        mCurrentTabGroupModelFilterSupplier.set(mTabGroupModelFilter);
        doReturn(mProfile).when(mTabModel).getProfile();
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(POSITION1).when(mTabGroupModelFilter).representativeIndexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).representativeIndexOf(mTab2);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        when(mTabModel.getTabRemover()).thenReturn(mTabRemover);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        doReturn(mTab1).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getRepresentativeTabAt(POSITION2);
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).getCurrentRepresentativeTabIndex();
        doReturn(POSITION1).when(mTabModel).index();
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTab1).when(mTabModel).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabModel).getTabAt(POSITION2);
        doReturn(mTab1).when(mTabModel).getTabById(TAB1_ID);
        doReturn(mTab2).when(mTabModel).getTabById(TAB2_ID);
        doReturn(POSITION1).when(mTabModel).indexOf(mTab1);
        doReturn(POSITION2).when(mTabModel).indexOf(mTab2);
        doNothing().when(mTabGroupModelFilter).addObserver(mTabModelObserverCaptor.capture());
        doNothing()
                .when(mTabGroupModelFilter)
                .addTabGroupObserver(mTabGroupModelFilterObserverCaptor.capture());
        doReturn(mView).when(mAnimationSourceViewProvider).getAnimationSourceViewForTab(anyInt());
        doReturn(mEditable).when(mTitleTextView).getText();
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mEditable).toString();
        doReturn(null).when(mRecyclerViewPositionSupplier).get();

        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mModel = spy(new PropertyModel(TabGridDialogProperties.ALL_KEYS));
        remakeMediator(/* withResetHandler= */ true, /* withAnimSource= */ true);

        mMediator.initWithNative(() -> mTabListEditorController);
        assertThat(mTabModelObserverCaptor.getAllValues().isEmpty(), equalTo(false));
        assertThat(mTabGroupModelFilterObserverCaptor.getAllValues().isEmpty(), equalTo(false));
    }

    @Test
    public void setupListenersAndObservers() {
        // These listeners and observers should be setup when the mediator is created.
        assertThat(
                mModel.get(TabGridDialogProperties.COLLAPSE_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
        assertThat(
                mModel.get(TabGridDialogProperties.ADD_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));
    }

    @Test
    public void setupShareListeners() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        mModel.get(TabGridDialogProperties.SHARE_BUTTON_CLICK_LISTENER).onClick(null);
        verify(mDataSharingTabManager)
                .createOrManageFlow(eq(EITHER_LOCAL_TAB_GROUP_ID), anyInt(), any());

        mModel.get(TabGridDialogProperties.SHARE_IMAGE_TILES_CLICK_LISTENER).onClick(null);
        verify(mDataSharingTabManager, times(2))
                .createOrManageFlow(eq(EITHER_LOCAL_TAB_GROUP_ID), anyInt(), any());

        mModel.get(TabGridDialogProperties.SEND_FEEDBACK_RUNNABLE).run();
        ArgumentCaptor<String> categoryCaptor = ArgumentCaptor.forClass(String.class);
        verify(mHelpAndFeedbackLauncher)
                .showFeedback(eq(mActivity), isNull(), categoryCaptor.capture());
        assertTrue(
                categoryCaptor
                        .getValue()
                        .contains(TabGridDialogMediator.SHARE_FEEDBACK_CATEGORY_SUFFIX));
    }

    @Test
    public void setupTabGroupTitleEdit() {
        // Setup editable title.
        assertThat(
                mMediator.getKeyboardVisibilityListenerForTesting(),
                instanceOf(KeyboardVisibilityDelegate.KeyboardVisibilityListener.class));
        assertThat(
                mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER),
                instanceOf(TextWatcher.class));
        assertThat(
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER),
                instanceOf(View.OnFocusChangeListener.class));
    }

    @Test
    public void setupTabListEditor() {
        // Setup selection editor for multiple items.
        assertThat(
                mModel.get(TabGridDialogProperties.MENU_CLICK_LISTENER),
                instanceOf(View.OnClickListener.class));

        ArgumentCaptor<List<TabListEditorAction>> captor =
                ArgumentCaptor.forClass((Class) List.class);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mMediator.onToolbarMenuItemClick(
                R.id.select_tabs,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mTabListEditorController).configureToolbarWithMenuItems(captor.capture());
        verify(mRecyclerViewPositionSupplier, times(1)).get();
        verify(mTabListEditorController).show(any(), eq(new ArrayList<>()), eq(null));
        List<TabListEditorAction> actions = captor.getValue();
        assertThat(actions.get(0), instanceOf(TabListEditorSelectionAction.class));
        assertThat(actions.get(1), instanceOf(TabListEditorCloseAction.class));
        assertThat(actions.get(2), instanceOf(TabListEditorUngroupAction.class));
    }

    @Test
    public void onClickAdd_HasCurrentTab() {
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        View.OnClickListener listener = mModel.get(TabGridDialogProperties.ADD_CLICK_LISTENER);
        listener.onClick(mView);

        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        verify(mDialogController).resetWithListOfTabs(null);
        verify(mTabCreator)
                .createNewTab(
                        isA(LoadUrlParams.class), eq(TabLaunchType.FROM_TAB_GROUP_UI), eq(mTab1));
    }

    @Test
    public void onClickAdd_NoCurrentTab() {
        mMediator.setCurrentTabIdForTesting(Tab.INVALID_TAB_ID);

        View.OnClickListener listener = mModel.get(TabGridDialogProperties.ADD_CLICK_LISTENER);
        listener.onClick(mView);

        verify(mTabCreator).launchNtp();
    }

    @Test
    public void onClickCollapse() {
        // Show the group of {tab1, tab2} in dialog to trigger the set of scrim observer.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);
        assertTrue(mMediator.onReset(tabGroup));

        View.OnClickListener listener = mModel.get(TabGridDialogProperties.COLLAPSE_CLICK_LISTENER);
        listener.onClick(mView);

        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void onClickScrim() {
        // Show the group of {tab1, tab2} in dialog to trigger the set of scrim observer.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);
        assertTrue(mMediator.onReset(tabGroup));

        Runnable scrimClickRunnable = mModel.get(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE);
        scrimClickRunnable.run();

        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void onTitleTextChange_WithoutFocus() {
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        // Mock tab1 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);
        assertThat(mEditable.toString(), equalTo(CUSTOMIZED_DIALOG_TITLE));

        textWatcher.afterTextChanged(mEditable);

        // mTabGroupModelFilter#setTabGroupTitle() should not react to text change when there is no
        // focus.
        verify(mTabGroupModelFilter, never()).setTabGroupTitle(anyInt(), anyString());
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(), equalTo(null));
    }

    @Test
    public void onTitleTextChange_WithFocus() {
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        // Mock tab1 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);
        assertThat(mEditable.toString(), equalTo(CUSTOMIZED_DIALOG_TITLE));

        // Focus on title TextView.
        View.OnFocusChangeListener listener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        listener.onFocusChange(mTitleTextView, true);

        textWatcher.afterTextChanged(mEditable);

        assertThat(
                mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void onTitleTextFocusChange() {
        View.OnFocusChangeListener listener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        assertThat(mMediator.getIsUpdatingTitleForTesting(), equalTo(false));

        listener.onFocusChange(mTitleTextView, true);

        assertThat(mMediator.getIsUpdatingTitleForTesting(), equalTo(true));
    }

    @Test
    public void onKeyBoardVisibilityChanged_updateTextAndKeyboard() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener listener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        mModel.set(TabGridDialogProperties.TITLE_CURSOR_VISIBILITY, false);
        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, false);

        listener.keyboardVisibilityChanged(true);
        assertThat(mModel.get(TabGridDialogProperties.TITLE_CURSOR_VISIBILITY), equalTo(true));
        assertThat(mModel.get(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED), equalTo(false));
        assertThat(mModel.get(TabGridDialogProperties.IS_KEYBOARD_VISIBLE), equalTo(false));

        listener.keyboardVisibilityChanged(false);
        assertThat(mModel.get(TabGridDialogProperties.TITLE_CURSOR_VISIBILITY), equalTo(false));
        assertThat(mModel.get(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED), equalTo(false));
        assertThat(mModel.get(TabGridDialogProperties.IS_KEYBOARD_VISIBLE), equalTo(false));
    }

    @Test
    public void onKeyBoardVisibilityChanged_StoreGroupTitle() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener keyboardVisibilityListener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);

        // Mock that keyboard shows and group title is updated.
        keyboardVisibilityListener.keyboardVisibilityChanged(true);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(
                mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void onKeyBoardVisibilityChanged_StoreGroupTitle_SingleTab() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener keyboardVisibilityListener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1)), TAB1_ID, TAB_GROUP_ID);

        // Mock that keyboard shows and group title is updated.
        keyboardVisibilityListener.keyboardVisibilityChanged(true);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(
                mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void onKeyBoardVisibilityChanged_NoFocus_NotStoreGroupTitle() {
        KeyboardVisibilityDelegate.KeyboardVisibilityListener keyboardVisibilityListener =
                mMediator.getKeyboardVisibilityListenerForTesting();
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);

        // Mock that keyboard shows but title edit text is not focused.
        keyboardVisibilityListener.keyboardVisibilityChanged(true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(mMediator.getIsUpdatingTitleForTesting(), equalTo(false));

        keyboardVisibilityListener.keyboardVisibilityChanged(false);

        verify(mTabGroupModelFilter, never()).setTabGroupTitle(anyInt(), anyString());
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void tabAddition() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_CHROME_UI,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();

        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void tabAddition_NoHide() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        when(mTabGroupModelFilter.isTabModelRestored()).thenReturn(true);
        mTabModelObserverCaptor
                .getValue()
                .didAddTab(
                        newTab,
                        TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP,
                        TabCreationState.LIVE_IN_FOREGROUND,
                        false);

        assertTrue(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));
    }

    @Test
    public void tabClosure_NotLast_NotCurrent() {
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);
        // Mock that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB1_ID);
        // Mock tab1 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        // Mock dialog title is null and the dialog is showing.
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, true);

        // Current tab ID should not update.
        assertThat(mMediator.getCurrentTabIdForTesting(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithListOfTabs(mTabList);
    }

    @Test
    public void tabClosure_NotLast_Current() {
        // Mock that tab1 and tab2 are in the same group, but tab2 just gets closed.
        doReturn(new ArrayList<>(Arrays.asList(mTab1)))
                .when(mTabGroupModelFilter)
                .getRelatedTabList(TAB2_ID);
        // Mock tab2 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB2_ID);
        // Mock dialog title is null and the dialog is showing.
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, true);

        // Current tab ID should be updated to TAB1_ID now.
        assertThat(mMediator.getCurrentTabIdForTesting(), equalTo(TAB1_ID));
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithListOfTabs(mTabList);
    }

    @Test
    public void tabClosure_Last_Current() {
        // Mock that tab1 is the last tab in the group and it just gets closed.
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        // As last tab in the group, tab1 is definitely the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        // Mock the dialog is showing and the animation source view is not null.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab1, true);

        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        verify(mDialogController).resetWithListOfTabs(null);
        verify(mTabSwitcherResetHandler, never()).resetWithListOfTabs(mTabList);

        assertFalse(mMediator.onReset(null));
        assertThat(mMediator.getCurrentTabIdForTesting(), equalTo(Tab.INVALID_TAB_ID));
    }

    @Test
    public void tabClosure_NonRootTab_StillGroupAfterClosure_WithStoredTitle() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);
        // Mock that tab1, tab2 and newTab are in the same group and tab1 is the root tab.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        // Mock that newTab just get closed.
        List<Tab> tabGroupAfterClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(tabGroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabGroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Mock that newTab is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB3_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupModelFilter).getTabGroupTitle(TAB1_ID);

        assertThat(
                mTabGroupModelFilter.getTabGroupTitle(mTab1.getRootId()),
                equalTo(CUSTOMIZED_DIALOG_TITLE));
        mTabModelObserverCaptor.getValue().willCloseTab(newTab, true);

        // Dialog title should still be the stored title.
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void tabClosure_RootTab_StillGroupAfterClosure_WithStoredTitle() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);
        // Mock that tab1, tab2 and newTab are in the same group and newTab is the root tab.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabGroup, TAB3_ID, TAB_GROUP_ID);

        // Mock that newTab just get closed.
        List<Tab> tabGroupAfterClosure = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        doReturn(tabGroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabGroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Mock that newTab is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB3_ID);

        // Mock that we have a stored title stored with reference to root ID of newTab.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupModelFilter).getTabGroupTitle(TAB3_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(newTab, true);

        // Dialog title should still be the stored title even if the root tab is closed.
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void tabClosure_SingleTabAfterClosure_WithStoredTitle_SingleTabGroupSupported() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);
        // Mock that tab1, tab2 are in the same group and tab1 is the root tab.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        // Mock that tab2 just get closed.
        List<Tab> tabGroupAfterClosure = new ArrayList<>(Arrays.asList(mTab1));
        doReturn(tabGroupAfterClosure).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Mock that tab2 is the current tab for the dialog.
        mMediator.setCurrentTabIdForTesting(TAB2_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupModelFilter).getTabGroupTitle(TAB1_ID);

        mTabModelObserverCaptor.getValue().willCloseTab(mTab2, true);

        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void tabClosureUndone() {
        // Mock that the dialog is showing.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        verify(mTabSwitcherResetHandler).resetWithListOfTabs(mTabList);
        ShadowLooper.runUiThreadTasks();
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB1_ID));
    }

    @Test
    public void tabClosureUndone_WithStoredTitle() {
        // Mock that the dialog is showing.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupModelFilter).getTabGroupTitle(TAB1_ID);

        // Mock that tab1 and tab2 are in the same group, and we are undoing tab2.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        // If current group has a stored title, dialog title should be set to stored title when
        // undoing a closure.
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
        verify(mTabSwitcherResetHandler).resetWithListOfTabs(mTabList);
        ShadowLooper.runUiThreadTasks();
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB2_ID));
    }

    @Test
    public void tabClosureUndone_WithDialogHidden() {
        // Mock that the dialog is hidden.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);

        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab1);

        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE1));
        // Dialog should still be hidden.
        assertThat(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE), equalTo(false));
        verify(mTabSwitcherResetHandler, never()).resetWithListOfTabs(mTabList);
        ShadowLooper.runUiThreadTasks();
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB1_ID));
    }

    @Test
    public void tabClosureCommitted() {
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

        ShadowLooper.runUiThreadTasks();
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB1_ID));
    }

    @Test
    public void onFinishingMultipleTabClosure() {
        List<Tab> tabs = Arrays.asList(mTab1, mTab2);
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(tabs, /* canRestore= */ true);

        ShadowLooper.runUiThreadTasks();
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(tabs));
    }

    @Test
    public void onFinishingMultipleTabClosure_singleTab() {
        List<Tab> tabs = Arrays.asList(mTab1);
        mTabModelObserverCaptor
                .getValue()
                .onFinishingMultipleTabClosure(tabs, /* canRestore= */ true);

        ShadowLooper.runUiThreadTasks();
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator), eq(TAB1_ID));
    }

    @Test
    public void allTabsClosureCommitted() {
        mTabModelObserverCaptor.getValue().allTabsClosureCommitted(false);

        ShadowLooper.runUiThreadTasks();
        verify(mSnackbarManager).dismissSnackbars(eq(mMediator));
    }

    @Test
    public void tabPendingClosure_DialogVisible() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().tabPendingClosure(mTab1);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void tabPendingClosure_DialogInvisible() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);

        mTabModelObserverCaptor.getValue().tabPendingClosure(mTab1);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void multipleTabsPendingClosure_DialogVisible() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor
                .getValue()
                .multipleTabsPendingClosure(Arrays.asList(mTab1, mTab2), false);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void multipleTabsPendingClosure_singleTab_DialogVisible() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor.getValue().multipleTabsPendingClosure(Arrays.asList(mTab1), false);

        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void multipleTabsPendingClosure_DialogInvisible() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);

        mTabModelObserverCaptor
                .getValue()
                .multipleTabsPendingClosure(Arrays.asList(mTab1, mTab2), false);

        verify(mSnackbarManager, never()).showSnackbar(any(Snackbar.class));
    }

    @Test
    public void tabSelection_stripContext() {
        remakeMediator(/* withResetHandler= */ false, /* withAnimSource= */ true);

        mMediator.initWithNative(() -> mTabListEditorController);
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab1, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();

        verify(mDialogController).resetWithListOfTabs(null);
    }

    @Test
    public void tabSelection_tabSwitcherContext() {
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mTabModelObserverCaptor
                .getValue()
                .didSelectTab(mTab1, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        assertTrue(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));
    }

    @Test
    public void hideDialog_FadeOutAnimation() {
        // Mock that the animation source view is null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mMediator.hideDialog(false);

        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();

        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    public void hideDialog_FadeOutAnimation_ClearsViewAnimation() {
        // Mock that the animation source view is set, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mMediator.hideDialog(false);

        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();

        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    public void hideDialog_ZoomOutAnimation() {
        // Mock that the animation source view is null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mMediator.hideDialog(true);

        // Animation source view should be specified.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    public void hideDialog_ForcesAnimationToFinish() {
        // Mock that the animation source view is null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mMediator.hideDialog(true);

        // Animation source view should be specified.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        mMediator.hideDialog(false);
        verify(mModel).set(TabGridDialogProperties.FORCE_ANIMATION_TO_FINISH, true);
        assertFalse(mModel.get(TabGridDialogProperties.FORCE_ANIMATION_TO_FINISH));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        verify(mDialogController).resetWithListOfTabs(eq(null));
    }

    @Test
    public void hideDialog_StoreModifiedGroupTitle() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a modified group title before dialog is hidden.
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(
                mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        mMediator.hideDialog(false);

        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void shareFlowStart_StoreModifiedGroupTitle() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a modified group title before share button is clicked.
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(
                mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        // Click share button.
        mModel.get(TabGridDialogProperties.SHARE_BUTTON_CLICK_LISTENER).onClick(null);

        // Verify that the modified title is committed to the TabGroupModelFilter.
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void hideDialog_ModifiedGroupTitleEmpty() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a modified group title which is an empty string.
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        doReturn("").when(mEditable).toString();
        textWatcher.afterTextChanged(mEditable);
        assertThat(mMediator.getCurrentGroupModifiedTitleForTesting(), equalTo(""));

        mMediator.hideDialog(false);

        // When updated title is a empty string, delete stored title and restore default title in
        // PropertyModel.
        verify(mTabGroupModelFilter).deleteTabGroupTitle(eq(TAB1_ID));
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
        verify(mTabGroupModelFilter).setTabGroupTitle(eq(TAB1_ID), eq(null));
    }

    @Test
    public void hideDialog_NoModifiedGroupTitle() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);

        mMediator.hideDialog(false);

        // When title is not updated, don't store title when hide dialog.
        verify(mTabGroupModelFilter, never()).setTabGroupTitle(anyInt(), anyString());
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void hideDialog_ClosingLastTab_SkipStoreGroupTitle() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that the last tab in the group is closed.
        doReturn(new ArrayList<>()).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        // Mock that we have a modified group title before dialog is hidden.
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(
                mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        mMediator.hideDialog(false);

        // Skip storing dialog title when the last tab is closing.
        verify(mTabGroupModelFilter, never()).setTabGroupTitle(anyInt(), anyString());
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void hideDialog_SingleTab_SkipStoreGroupTitle() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is now a single tab.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1)), TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a modified group title before dialog is hidden.
        TextWatcher textWatcher = mModel.get(TabGridDialogProperties.TITLE_TEXT_WATCHER);
        View.OnFocusChangeListener onFocusChangeListener =
                mModel.get(TabGridDialogProperties.TITLE_TEXT_ON_FOCUS_LISTENER);
        onFocusChangeListener.onFocusChange(mTitleTextView, true);
        textWatcher.afterTextChanged(mEditable);
        assertThat(
                mMediator.getCurrentGroupModifiedTitleForTesting(),
                equalTo(CUSTOMIZED_DIALOG_TITLE));

        mMediator.hideDialog(false);

        // Skip storing dialog title when this group becomes a single tab.
        verify(mTabGroupModelFilter, never()).setTabGroupTitle(anyInt(), anyString());
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void hideDialog_withTabGroupContinuation() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        mMediator.hideDialog(false);

        verify(mTabListEditorController).hide();
    }

    @Test
    public void onReset_hideDialog() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        assertFalse(mMediator.onReset(null));

        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        verify(mDialogController).postHiding();
    }

    @Test
    public void onReset_DialogNotVisible_NoOp() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);

        assertFalse(mMediator.onReset(null));

        verifyNoMoreInteractions(mDialogController);
    }

    @Test
    public void finishedHiding() {
        mMediator.finishedHidingDialogView();

        verify(mDialogController).resetWithListOfTabs(null);
        verify(mDialogController).postHiding();
    }

    @Test
    public void showDialog_FromGts_setSelectedColor() {
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a stored color stored with reference to root ID of tab1.
        // when(mTabGroupModelFilter.getTabGroupColor(mTab1.getRootId())).thenReturn(COLOR_2);
        mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, COLOR_2);

        assertTrue(mMediator.onReset(tabGroup));
        mMediator.setSelectedTabGroupColor(COLOR_3);

        // Assert that the color has changed both in the property model and the model filter.
        assertThat(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), equalTo(COLOR_3));
        verify(mTabGroupModelFilter).setTabGroupColor(mTab1.getRootId(), COLOR_3);
    }

    @Test
    public void showDialog_FromGts() {
        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mModel.set(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        when(mTabGroupModelFilter.getTabGroupColorWithFallback(mTab1.getRootId()))
                .thenReturn(COLOR_2);
        assertTrue(mMediator.onReset(tabGroup));

        // Assert that a color and the incognito status were set.
        assertThat(mModel.get(TabGridDialogProperties.IS_INCOGNITO), equalTo(false));
        assertThat(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), equalTo(COLOR_2));

        assertNull(mModel.get(TabGridDialogProperties.ANIMATION_BACKGROUND_COLOR));

        assertThat(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim click runnable should be set as the current scrim runnable.
        assertThat(
                mModel.get(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
        // Animation source view should be updated with specific view.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        // Dialog title should be updated.
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
        // Prepare dialog invoked.
        verify(mDialogController).prepareDialog();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.FORCE_LIST_TAB_SWITCHER})
    @DisableFeatures({ChromeFeatureList.DISABLE_LIST_TAB_SWITCHER})
    public void showDialog_FromListGts() {
        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mModel.set(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        when(mTabGroupModelFilter.getTabGroupColorWithFallback(mTab1.getRootId()))
                .thenReturn(COLOR_2);
        assertTrue(mMediator.onReset(tabGroup));

        // Assert that a color and the incognito status were set.
        assertThat(mModel.get(TabGridDialogProperties.IS_INCOGNITO), equalTo(false));
        assertThat(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), equalTo(COLOR_2));

        int backgroundColor =
                TabUiThemeUtils.getCardViewBackgroundColor(
                        mActivity, /* isIncognito= */ false, /* isSelected= */ false);
        assertEquals(
                mModel.get(TabGridDialogProperties.ANIMATION_BACKGROUND_COLOR).intValue(),
                backgroundColor);

        assertThat(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim click runnable should be set as the current scrim runnable.
        assertThat(
                mModel.get(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
        // Animation source view should be updated with specific view.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        // Dialog title should be updated.
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
        // Prepare dialog invoked.
        verify(mDialogController).prepareDialog();
    }

    @Test
    public void showDialog_FromGts_WithStoredTitle() {
        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mModel.set(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupModelFilter).getTabGroupTitle(TAB1_ID);

        assertTrue(mMediator.onReset(tabGroup));

        assertThat(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim click runnable should be set as the current scrim runnable.
        assertThat(
                mModel.get(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
        // Animation source view should be updated with specific view.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(mView));
        // Dialog title should be updated with stored title.
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void showDialog_FromStrip() {
        // For strip we don't play zoom-in/zoom-out for show/hide dialog, and thus
        // the animationParamsProvider is null.
        remakeMediator(/* withResetHandler= */ true, /* withAnimSource= */ false);
        mMediator.initWithNative(() -> mTabListEditorController);

        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mModel.set(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        when(mTabGroupModelFilter.getTabGroupColorWithFallback(mTab1.getRootId()))
                .thenReturn(COLOR_2);
        assertTrue(mMediator.onReset(tabGroup));

        // Assert that a color and the incognito status were set.
        assertThat(mModel.get(TabGridDialogProperties.IS_INCOGNITO), equalTo(false));
        assertThat(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), equalTo(COLOR_2));

        assertThat(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim observer should be set as the current scrim runnable.
        assertThat(
                mModel.get(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        // Dialog title should be updated.
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
        // Prepare dialog invoked.
        verify(mDialogController).prepareDialog();
    }

    @Test
    public void showDialog_FromStrip_WithStoredTitle() {
        // For strip we don't play zoom-in/zoom-out for show/hide dialog, and thus
        // the animationParamsProvider is null.
        remakeMediator(/* withResetHandler= */ true, /* withAnimSource= */ false);
        mMediator.initWithNative(() -> mTabListEditorController);
        // Mock that the dialog is hidden and animation source view, header title and scrim click
        // runnable are all null.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, null);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, null);
        mModel.set(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE, null);
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a stored title stored with reference to root ID of tab1.
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupModelFilter).getTabGroupTitle(TAB1_ID);

        assertTrue(mMediator.onReset(tabGroup));

        assertThat(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Scrim observer should be set as the current scrim click runnable.
        assertThat(
                mModel.get(TabGridDialogProperties.SCRIMVIEW_CLICK_RUNNABLE),
                equalTo(mMediator.getScrimClickRunnableForTesting()));
        // Animation source view should not be specified.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        // Dialog title should be updated with stored title.
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
    }

    @Test
    public void showDialog_FromStrip_SetupAnimation() {
        // For strip we don't play zoom-in/zoom-out for show/hide dialog, and thus
        // the animationParamsProvider is null.
        remakeMediator(/* withResetHandler= */ true, /* withAnimSource= */ false);
        mMediator.initWithNative(() -> mTabListEditorController);
        // Mock that the dialog is hidden and animation source view is set to some mock view for
        // testing purpose.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mock(View.class));
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        assertTrue(mMediator.onReset(tabGroup));

        assertThat(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Animation source view should be set to null so that dialog will setup basic animation.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
    }

    @Test
    public void testDialogToolbarMenu_SelectionMode() {
        // Mock that currently the title text is focused and the keyboard is showing. The current
        // tab is tab1 which is in a group of {tab1, tab2}.
        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, true);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        mMediator.onToolbarMenuItemClick(
                R.id.select_tabs,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);

        assertThat(mModel.get(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED), equalTo(false));
        verify(mRecyclerViewPositionSupplier, times(1)).get();
        verify(mTabListEditorController).show(eq(tabGroup), eq(new ArrayList<>()), eq(null));
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.SelectTabs"));
    }

    @Test
    public void testDialogToolbarMenu_EditGroupName() {
        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, false);

        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        mMediator.onToolbarMenuItemClick(
                R.id.edit_group_name,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        assertTrue(mModel.get(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED));
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.Rename"));
    }

    @Test
    public void testDialogToolbarMenu_EditGroupColor() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        mMediator.onToolbarMenuItemClick(
                R.id.edit_group_color,
                TAB_GROUP_ID,
                /* collaborationId= */ null,
                /* listViewTouchTracker= */ null);
        verify(mShowColorPickerPopupRunnable).run();
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.EditColor"));
    }

    @Test
    public void testDialogToolbarMenu_CloseGroup_NullListViewTouchTracker() {
        testDialogToolbarMenu_CloseOrDeleteGroup(
                R.id.close_tab_group,
                /* listViewTouchTracker= */ null,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ true,
                /* expectedUserAction= */ "TabGridDialogMenu.Close");
    }

    @Test
    public void testDialogToolbarMenu_CloseGroup_ClickWithTouch() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testDialogToolbarMenu_CloseOrDeleteGroup(
                R.id.close_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ true,
                /* expectedUserAction= */ "TabGridDialogMenu.Close");
    }

    @Test
    public void testDialogToolbarMenu_CloseGroup_ClickWithMouse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testDialogToolbarMenu_CloseOrDeleteGroup(
                R.id.close_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ false,
                /* shouldHideTabGroups= */ true,
                /* expectedUserAction= */ "TabGridDialogMenu.Close");
    }

    @Test
    public void testDialogToolbarMenu_DeleteGroup_NullListViewTouchTracker() {
        testDialogToolbarMenu_CloseOrDeleteGroup(
                R.id.delete_tab_group,
                /* listViewTouchTracker= */ null,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ false,
                /* expectedUserAction= */ "TabGridDialogMenu.Delete");
    }

    @Test
    public void testDialogToolbarMenu_DeleteGroup_ClickWithTouch() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createTouchMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testDialogToolbarMenu_CloseOrDeleteGroup(
                R.id.delete_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ true,
                /* shouldHideTabGroups= */ false,
                /* expectedUserAction= */ "TabGridDialogMenu.Delete");
    }

    @Test
    public void testDialogToolbarMenu_DeleteGroup_ClickWithMouse() {
        long downMotionTime = SystemClock.uptimeMillis();
        FakeListViewTouchTracker listViewTouchTracker = new FakeListViewTouchTracker();
        listViewTouchTracker.setLastSingleTapUpInfo(
                MotionEventTestUtils.createMouseMotionInfo(
                        downMotionTime,
                        /* eventTime= */ downMotionTime + 50,
                        MotionEvent.ACTION_UP));

        testDialogToolbarMenu_CloseOrDeleteGroup(
                R.id.delete_tab_group,
                listViewTouchTracker,
                /* shouldAllowUndo= */ false,
                /* shouldHideTabGroups= */ false,
                /* expectedUserAction= */ "TabGridDialogMenu.Delete");
    }

    private void testDialogToolbarMenu_CloseOrDeleteGroup(
            @IdRes int menuId,
            @Nullable ListViewTouchTracker listViewTouchTracker,
            boolean shouldAllowUndo,
            boolean shouldHideTabGroups,
            String expectedUserAction) {
        assertTrue(menuId == R.id.close_tab_group || menuId == R.id.delete_tab_group);

        // Setup
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        // Act
        mMediator.onToolbarMenuItemClick(
                menuId, TAB_GROUP_ID, /* collaborationId= */ null, listViewTouchTracker);

        // Assert
        verify(mTabRemover)
                .closeTabs(
                        eq(
                                TabClosureParams.forCloseTabGroup(
                                                mTabGroupModelFilter, TAB_GROUP_ID)
                                        .allowUndo(shouldAllowUndo)
                                        .hideTabGroups(shouldHideTabGroups)
                                        .build()),
                        /* allowDialog= */ eq(true),
                        any());
        assertEquals(1, mActionTester.getActionCount(expectedUserAction));
    }

    @Test
    public void testDialogToolbarMenu_ManageSharing() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        mMediator.onToolbarMenuItemClick(
                R.id.manage_sharing,
                TAB_GROUP_ID,
                COLLABORATION_ID1,
                /* listViewTouchTracker= */ null);
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.ManageSharing"));
        verify(mDataSharingTabManager)
                .createOrManageFlow(eq(EITHER_LOCAL_TAB_GROUP_ID), anyInt(), eq(null));
    }

    @Test
    public void testDialogToolbarMenu_RecentActivity() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        mMediator.onToolbarMenuItemClick(
                R.id.recent_activity,
                TAB_GROUP_ID,
                COLLABORATION_ID1,
                /* listViewTouchTracker= */ null);
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.RecentActivity"));
        verify(mDataSharingTabManager).showRecentActivity(mActivity, COLLABORATION_ID1);
        verifyClearDirtyMessagesForGroup();
    }

    @Test
    public void testDialogToolbarMenu_DeleteSharedGroup() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        when(mTabGroupModelFilter.getTabGroupTitle(anyInt())).thenReturn(GROUP_TITLE);
        CoreAccountInfo coreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId(EMAIL1, GAIA_ID1);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.OWNER);

        mMediator.onToolbarMenuItemClick(
                R.id.delete_shared_group,
                TAB_GROUP_ID,
                COLLABORATION_ID1,
                /* listViewTouchTracker= */ null);
        verify(mDataSharingTabManager).leaveOrDeleteFlow(eq(EITHER_LOCAL_TAB_GROUP_ID), anyInt());
    }

    @Test
    public void testDialogToolbarMenu_LeaveSharedGroup() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1, GROUP_MEMBER2);

        when(mTabGroupModelFilter.getTabGroupTitle(anyInt())).thenReturn(GROUP_TITLE);
        CoreAccountInfo coreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId(EMAIL2, GAIA_ID2);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);
        when(mCollaborationService.getCurrentUserRoleForGroup(COLLABORATION_ID1))
                .thenReturn(MemberRole.MEMBER);

        mMediator.onToolbarMenuItemClick(
                R.id.leave_group,
                TAB_GROUP_ID,
                COLLABORATION_ID1,
                /* listViewTouchTracker= */ null);
        verify(mDataSharingTabManager).leaveOrDeleteFlow(eq(EITHER_LOCAL_TAB_GROUP_ID), anyInt());
    }

    @Test
    public void testSnackbarController_onAction_singleTab() {
        mMediator.onAction(TAB1_ID);

        verify(mTabModel).cancelTabClosure(eq(TAB1_ID));
    }

    @Test
    public void testSnackbarController_onAction_multipleTabs() {
        mMediator.onAction(Arrays.asList(mTab1, mTab2));

        verify(mTabModel).cancelTabClosure(eq(TAB1_ID));
        verify(mTabModel).cancelTabClosure(eq(TAB2_ID));
    }

    @Test
    public void testSnackbarController_onDismissNoAction_singleTab() {
        mMediator.onDismissNoAction(TAB1_ID);

        verify(mTabModel).commitTabClosure(eq(TAB1_ID));
    }

    @Test
    public void testSnackbarController_onDismissNoAction_multipleTabs() {
        mMediator.onDismissNoAction(Arrays.asList(mTab1, mTab2));

        verify(mTabModel).commitTabClosure(eq(TAB1_ID));
        verify(mTabModel).commitTabClosure(eq(TAB2_ID));
    }

    @Test
    public void testScrollToTab() {
        // Mock that tab1, tab2 and newTab are in the same group and newTab is the root tab.
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2, newTab));
        createTabGroup(tabGroup, TAB2_ID, TAB_GROUP_ID);

        int groupIndex = 3;
        // Mock that mTab2 is the current tab for the dialog.
        when(mTabGroupModelFilter.representativeIndexOf(mTab1)).thenReturn(groupIndex);
        when(mTabGroupModelFilter.getRepresentativeTabAt(groupIndex)).thenReturn(mTab2);
        when(mTabGroupModelFilter.getCurrentRepresentativeTab()).thenReturn(mTab2);
        doReturn(tabGroup).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Reset and confirm scroll index.
        assertTrue(mMediator.onReset(tabGroup));

        assertEquals(1, mModel.get(TabGridDialogProperties.INITIAL_SCROLL_INDEX).intValue());
    }

    @Test
    public void testTabUngroupBarText() {
        // Mock that tab1 and tab2 are in the same group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        assertTrue(mMediator.onReset(tabGroup));
        // Check that the text indicates that this is not the last tab in the group.
        assertEquals(
                mActivity.getString(R.string.remove_tab_from_group),
                mModel.get(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT));

        // Mock that tab1 is the only tab that remains in the group.
        List<Tab> tabGroupAfterUngroup = new ArrayList<>(Arrays.asList(mTab1));
        doReturn(tabGroupAfterUngroup).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        assertTrue(mMediator.onReset(tabGroupAfterUngroup));
        // Check that the text indicates that this is the last tab in the group.
        assertEquals(
                mActivity.getString(R.string.remove_last_tab_action),
                mModel.get(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT));
    }

    @Test
    public void testTabUngroupBarText_Member() {
        when(mCollaborationService.getCurrentUserRoleForGroup(any())).thenReturn(MemberRole.MEMBER);
        // Mock that tab1 and tab2 are in the same group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        // Check that the text indicates that this is not the last tab in the group.
        assertEquals(
                mActivity.getString(R.string.remove_tab_from_group),
                mModel.get(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT));

        // Mock that tab1 is the only tab that remains in the group.
        List<Tab> tabGroupAfterUngroup = new ArrayList<>(Arrays.asList(mTab1));
        doReturn(tabGroupAfterUngroup).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        assertTrue(mMediator.onReset(tabGroupAfterUngroup));
        // Check that the text indicates that this is the last tab in the group.
        assertEquals(
                mActivity.getString(R.string.remove_last_tab_action_member),
                mModel.get(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT));
    }

    @Test
    public void testTabGroupColorUpdated() {
        int rootId = TAB1_ID;
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, rootId, TAB_GROUP_ID);

        assertTrue(mMediator.onReset(tabGroup));

        assertNotEquals(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), COLOR_3);

        mTabGroupModelFilterObserverCaptor.getValue().didChangeTabGroupColor(rootId, COLOR_3);

        assertThat(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), equalTo(COLOR_3));
    }

    @Test
    public void testTabGroupTitleUpdated() {
        int rootId = TAB1_ID;
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, rootId, TAB_GROUP_ID);

        when(mTabGroupModelFilter.getTabGroupTitle(rootId)).thenReturn(CUSTOMIZED_DIALOG_TITLE);
        assertTrue(mMediator.onReset(tabGroup));

        assertEquals(CUSTOMIZED_DIALOG_TITLE, mModel.get(TabGridDialogProperties.HEADER_TITLE));

        String newTitle = "BAR";
        when(mTabGroupModelFilter.getTabGroupTitle(rootId)).thenReturn(newTitle);
        mTabGroupModelFilterObserverCaptor.getValue().didChangeTabGroupTitle(rootId, newTitle);

        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(newTitle));
    }

    @Test
    public void destroy() {
        mMediator.destroy();

        verify(mTabGroupModelFilter).removeObserver(mTabModelObserverCaptor.capture());
        assertFalse(mCurrentTabGroupModelFilterSupplier.hasObservers());
        verify(mDesktopWindowStateManager).removeObserver(mMediator);
        verify(mMessagingBackendService).removePersistentMessageObserver(any());
    }

    @Test
    public void testUpdateShareData_Incognito() {
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SEND_FEEDBACK));

        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        verify(mSharedImageTilesCoordinator)
                .onGroupMembersChanged(COLLABORATION_ID1, List.of(GROUP_MEMBER1));

        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        resetForDataSharing(/* isShared= */ false);

        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SEND_FEEDBACK));
        verify(mSharedImageTilesCoordinator).onGroupMembersChanged(null, null);
    }

    @Test
    public void testShowOrUpdateCollaborationActivityMessageCard() {
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));

        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.DATA_SHARING)
                .param(TabGridDialogMediator.SHOW_SEND_FEEDBACK_PARAM, true)
                .apply();

        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(true);
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1, GROUP_MEMBER2);
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertTrue(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        assertTrue(mModel.get(TabGridDialogProperties.SHOW_SEND_FEEDBACK));
        verify(mSharedImageTilesCoordinator)
                .onGroupMembersChanged(COLLABORATION_ID1, List.of(GROUP_MEMBER1, GROUP_MEMBER2));
        verify(mDialogController, never()).addMessageCardItem(/* position= */ eq(0), any());

        // Reset with null first as reusing the same TabGroupId does not reset the observer.
        when(mServiceStatus.isAllowedToCreate()).thenReturn(false);
        assertFalse(mMediator.onReset(null));
        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(false);
        resetForDataSharing(/* isShared= */ false);
        verify(mDialogController).removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
        verify(mSharedImageTilesCoordinator).onGroupMembersChanged(null, null);
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertEquals(
                R.string.tab_grid_share_button_text,
                mModel.get(TabGridDialogProperties.SHARE_BUTTON_STRING_RES));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        when(mServiceStatus.isAllowedToCreate()).thenReturn(true);

        // Reset with null first as reusing the same TabGroupId does not reset the observer.
        assertFalse(mMediator.onReset(null));
        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(false);
        resetForDataSharing(/* isShared= */ false);
        verify(mDialogController).removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
        verify(mSharedImageTilesCoordinator).onGroupMembersChanged(null, null);
        assertTrue(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertEquals(
                R.string.tab_grid_share_button_text,
                mModel.get(TabGridDialogProperties.SHARE_BUTTON_STRING_RES));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SEND_FEEDBACK));

        FeatureOverrides.newBuilder()
                .enable(ChromeFeatureList.DATA_SHARING)
                .param(TabGridDialogMediator.SHOW_SEND_FEEDBACK_PARAM, false)
                .apply();

        // Reset with null first as reusing the same TabGroupId does not reset the observer.
        assertFalse(mMediator.onReset(null));
        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(false);
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        verify(mDialogController)
                .addMessageCardItem(/* position= */ eq(0), mMessageCardModelCaptor.capture());
        assertTrue(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SEND_FEEDBACK));
        assertEquals(
                R.string.tab_grid_manage_button_text,
                mModel.get(TabGridDialogProperties.SHARE_BUTTON_STRING_RES));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        String text = mMessageCardModelCaptor.getValue().get(DESCRIPTION_TEXT).toString();
        assertTrue(text, text.contains("4"));

        reset(mDialogController);
        mockPersistentMessages(/* added= */ 1, /* navigated= */ 2, /* removed= */ 5);
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor
                .getValue()
                .displayPersistentMessage(makePersistentMessage(CollaborationEvent.TAB_REMOVED));
        verify(mDialogController)
                .addMessageCardItem(/* position= */ eq(0), mMessageCardModelCaptor.capture());
        text = mMessageCardModelCaptor.getValue().get(DESCRIPTION_TEXT).toString();
        assertFalse(text, text.contains("4"));
        assertTrue(text, text.contains("5"));

        reset(mDialogController);
        mockPersistentMessages(/* added= */ 0, /* navigated= */ 2, /* removed= */ 5);
        mPersistentMessageObserverCaptor
                .getValue()
                .hidePersistentMessage(makePersistentMessage(CollaborationEvent.TAB_ADDED));
        verify(mDialogController)
                .addMessageCardItem(/* position= */ eq(0), mMessageCardModelCaptor.capture());
        text = mMessageCardModelCaptor.getValue().get(DESCRIPTION_TEXT).toString();
        // Shows "new" as a combination of added (1 -> 0) and navigated (2 -> 2).
        assertFalse(text, text.contains("3"));
        assertTrue(text, text.contains("2"));
    }

    @Test
    public void testCollaborationActivityMessageCard_Dismiss() {
        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(false);
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        verify(mDialogController)
                .addMessageCardItem(
                        /* position= */ eq(0), mCollaborationActivityMessageCardCaptor.capture());

        mCollaborationActivityMessageCardCaptor
                .getValue()
                .get(MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER)
                .dismiss(MessageType.COLLABORATION_ACTIVITY);

        verify(mDialogController).removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
        verifyClearDirtyMessagesForGroup();
    }

    @Test
    public void testCollaborationActivityMessageCard_ClickNoCollaboration() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        verify(mDialogController)
                .addMessageCardItem(
                        /* position= */ eq(0), mCollaborationActivityMessageCardCaptor.capture());

        verify(mDataSharingService).addObserver(mSharingObserverCaptor.capture());
        mSharingObserverCaptor.getValue().onGroupRemoved(COLLABORATION_ID1);

        mCollaborationActivityMessageCardCaptor
                .getValue()
                .get(MESSAGE_SERVICE_ACTION_PROVIDER)
                .review();

        verify(mDataSharingTabManager, never()).showRecentActivity(any(), anyString());
        verify(mDialogController, atLeastOnce())
                .removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
        verifyClearDirtyMessagesForGroup();
    }

    @Test
    public void testBottomSheetTriggered() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE))
                .thenReturn(true);
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        assertTrue(mMediator.onReset(tabGroup));
        ShadowLooper.runUiThreadTasks();

        // We expect 2 invocations due to the #resetForDataSharing call.
        verify(mBottomSheetController, times(2)).requestShowContent(any(), eq(true));
        verify(mTracker, times(2))
                .shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE);
    }

    @Test
    public void testBottomSheetNotTriggered() {
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.TAB_GROUP_SHARE_NOTICE_FEATURE))
                .thenReturn(false);

        resetForDataSharing(/* isShared= */ false, GROUP_MEMBER1);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        assertTrue(mMediator.onReset(tabGroup));
        ShadowLooper.runUiThreadTasks();

        verify(mBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mTracker, never()).shouldTriggerHelpUi(anyString());
    }

    @Test
    public void testCollaborationActivityMessageCard_Click() {
        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(false);
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        verify(mDialogController)
                .addMessageCardItem(
                        /* position= */ eq(0), mCollaborationActivityMessageCardCaptor.capture());

        mCollaborationActivityMessageCardCaptor
                .getValue()
                .get(MESSAGE_SERVICE_ACTION_PROVIDER)
                .review();

        verify(mDataSharingTabManager).showRecentActivity(mActivity, COLLABORATION_ID1);
        verifyClearDirtyMessagesForGroup();
    }

    @Test
    public void testSetGridContentSensitivity() {
        assertFalse(mModel.get(TabGridDialogProperties.IS_CONTENT_SENSITIVE));
        mMediator.setGridContentSensitivity(/* contentIsSensitive= */ true);
        assertTrue(mModel.get(TabGridDialogProperties.IS_CONTENT_SENSITIVE));
        mMediator.setGridContentSensitivity(/* contentIsSensitive= */ false);
        assertFalse(mModel.get(TabGridDialogProperties.IS_CONTENT_SENSITIVE));
    }

    private void remakeMediator(boolean withResetHandler, boolean withAnimSource) {
        if (mMediator != null) {
            mMediator.destroy();
        }
        mMediator =
                new TabGridDialogMediator(
                        mActivity,
                        mDialogController,
                        mModel,
                        mCurrentTabGroupModelFilterSupplier,
                        withResetHandler ? mTabSwitcherResetHandler : null,
                        mRecyclerViewPositionSupplier,
                        withAnimSource ? mAnimationSourceViewProvider : null,
                        mSnackbarManager,
                        mBottomSheetController,
                        mSharedImageTilesCoordinator,
                        mDataSharingTabManager,
                        /* componentName= */ "",
                        mShowColorPickerPopupRunnable,
                        mModalDialogManager,
                        mDesktopWindowStateManager,
                        mTabBookmarkerSupplier,
                        mShareDelegateSupplier);
    }

    @Test
    public void onReset_NullAfterSharedGroup() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        assertFalse(mMediator.onReset(null));

        verify(mDialogController).removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
    }

    @Test
    public void onAppHeaderStateChange_setAppHeaderHeight() {
        // App header height not set.
        assertThat(mModel.get(TabGridDialogProperties.APP_HEADER_HEIGHT), equalTo(0));
        // Rect with height = 10.
        Rect headerRect = new Rect(0, 0, 10, 10);
        AppHeaderState state = new AppHeaderState(headerRect, headerRect, true);
        when(mDesktopWindowStateManager.getAppHeaderState()).thenReturn(state);

        mMediator.onAppHeaderStateChanged(state);

        assertThat(mModel.get(TabGridDialogProperties.APP_HEADER_HEIGHT), equalTo(10));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_BOTTOM_SHEET_ANDROID)
    public void onLongPress_tabGroupParityEnabled() {
        CancelLongPressTabItemEventListener cancelLongPress =
                mMediator.onLongPressEvent(TAB1_ID, mCardView, mTabGridContextMenuCoordinator);
        verify(mTabGridContextMenuCoordinator).showMenu(any(), eq(TAB1_ID));

        assertNotNull(cancelLongPress);
        cancelLongPress.cancelLongPress();
        verify(mTabGridContextMenuCoordinator).dismiss();
    }

    @Test
    public void onLongPress_tabGroupParityDisabled() {
        CancelLongPressTabItemEventListener cancelLongPress =
                mMediator.onLongPressEvent(TAB1_ID, mCardView);
        verify(mTabGridContextMenuCoordinator, never()).showMenu(any(), eq(TAB1_ID));
        assertNull(cancelLongPress);
    }

    @Test
    public void testSuppressAccessibility() {
        assertFalse(mModel.get(SUPPRESS_ACCESSIBILITY));

        verify(mBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        mBottomSheetObserverCaptor.getValue().onSheetOpened(StateChangeReason.NONE);
        assertTrue(mModel.get(SUPPRESS_ACCESSIBILITY));

        mBottomSheetObserverCaptor.getValue().onSheetClosed(StateChangeReason.NONE);
        assertFalse(mModel.get(SUPPRESS_ACCESSIBILITY));
    }

    private void resetForDataSharing(boolean isShared, GroupMember... members) {
        int rootId = TAB1_ID;
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, rootId, TAB_GROUP_ID);

        GroupData groupData = SharedGroupTestHelper.newGroupData(COLLABORATION_ID1, members);
        if (isShared) {
            when(mCollaborationService.getGroupData(COLLABORATION_ID1)).thenReturn(groupData);
        } else {
            when(mCollaborationService.getGroupData(any())).thenReturn(null);
        }

        setupSyncedGroup(isShared);

        assertTrue(mMediator.onReset(tabGroup));
        ShadowLooper.runUiThreadTasks();
        verify(mDataSharingService, atLeastOnce())
                .addObserver(mDataSharingServiceObserverCaptor.capture());
        DataSharingService.Observer observer = mDataSharingServiceObserverCaptor.getValue();
        assertNotNull(observer);
        if (isShared) {
            observer.onGroupChanged(groupData);
        } else {
            observer.onGroupRemoved(COLLABORATION_ID1);
        }
    }

    private void setupSyncedGroup(boolean isShared) {
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.title = GROUP_TITLE;
        savedTabGroup.collaborationId = isShared ? COLLABORATION_ID1 : null;
        when(mTabGroupSyncService.getGroup(any(LocalTabGroupId.class))).thenReturn(savedTabGroup);
    }

    private Tab prepareTab(int id, String title) {
        Tab tab = TabUiUnitTestUtils.prepareTab(id, title, GURL.emptyGURL());
        doReturn(true).when(tab).isIncognito();
        return tab;
    }

    private void createTabGroup(List<Tab> tabs, int rootId, Token tabGroupId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(mTabGroupModelFilter.getTabsInGroup(tabGroupId)).thenReturn(tabs);
            when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
            when(tab.getRootId()).thenReturn(rootId);
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
        }
    }

    private PersistentMessage makePersistentMessage(@CollaborationEvent int collaborationEvent) {
        MessageAttribution attribution = new MessageAttribution();
        attribution.tabMetadata = new TabMessageMetadata();
        attribution.tabMetadata.localTabId = TAB1_ID;
        attribution.tabGroupMetadata = new TabGroupMessageMetadata();
        attribution.tabGroupMetadata.localTabGroupId = LOCAL_TAB_GROUP_ID;
        PersistentMessage message = new PersistentMessage();
        message.attribution = attribution;
        message.collaborationEvent = collaborationEvent;
        return message;
    }

    private void mockPersistentMessages(int added, int navigated, int removed) {
        List<PersistentMessage> messageList = new ArrayList<>();
        for (int i = 0; i < added; i++) {
            messageList.add(makePersistentMessage(CollaborationEvent.TAB_ADDED));
        }
        for (int i = 0; i < navigated; i++) {
            messageList.add(makePersistentMessage(CollaborationEvent.TAB_UPDATED));
        }
        when(mMessagingBackendService.getMessagesForGroup(
                        any(), eq(Optional.of(PersistentNotificationType.DIRTY_TAB))))
                .thenReturn(messageList);

        List<PersistentMessage> tombstonedMessageList = new ArrayList<>();
        for (int i = 0; i < removed; i++) {
            tombstonedMessageList.add(makePersistentMessage(CollaborationEvent.TAB_REMOVED));
        }
        when(mMessagingBackendService.getMessages(
                        eq(Optional.of(PersistentNotificationType.TOMBSTONED))))
                .thenReturn(tombstonedMessageList);
    }

    private void verifyClearDirtyMessagesForGroup() {
        verify(mMessagingBackendService)
                .clearDirtyTabMessagesForGroup(
                        argThat(collaborationId -> collaborationId.equals(COLLABORATION_ID1)));
    }
}
