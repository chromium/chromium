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
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.DESCRIPTION_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_ACTION_PROVIDER;
import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_SERVICE_DISMISS_ACTION_PROVIDER;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.EMAIL2;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GAIA_ID2;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER1;
import static org.chromium.components.data_sharing.SharedGroupTestHelper.GROUP_MEMBER2;

import android.app.Activity;
import android.graphics.Rect;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.EditText;

import androidx.annotation.Nullable;

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

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_group_sync.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.desktop_windowing.AppHeaderState;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingService.GroupDataOrFailureOutcome;
import org.chromium.components.data_sharing.GroupMember;
import org.chromium.components.data_sharing.SharedGroupTestHelper;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.messaging.MessageAttribution;
import org.chromium.components.tab_group_sync.messaging.MessagingBackendService;
import org.chromium.components.tab_group_sync.messaging.PersistentMessage;
import org.chromium.components.tab_group_sync.messaging.UserAction;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for {@link TabGridDialogMediator}. */
@SuppressWarnings({"ArraysAsListWithZeroOrOneArgument", "ResultOfMethodCallIgnored"})
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.DATA_SHARING)
public class TabGridDialogMediatorUnitTest {
    private static final String TAB1_TITLE = "Tab1";
    private static final String TAB2_TITLE = "Tab2";
    private static final String TAB3_TITLE = "Tab3";
    private static final String DIALOG_TITLE1 = "1 tab";
    private static final String DIALOG_TITLE2 = "2 tabs";
    private static final String CUSTOMIZED_DIALOG_TITLE = "Cool Tabs";
    private static final String COLLABORATION_ID1 = "A";
    private static final String GROUP_TITLE = "My Group";
    private static final int COLOR_2 = 1;
    private static final int COLOR_3 = 2;
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final int TAB3_ID = 123;
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final Token TAB_GROUP_ID = new Token(1L, 2L);

    @Rule public JniMocker mJniMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private View mView;
    @Mock private TabGridDialogMediator.DialogController mDialogController;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabCreator mTabCreator;
    @Mock private TabSwitcherResetHandler mTabSwitcherResetHandler;
    @Mock private TabGridDialogMediator.AnimationSourceViewProvider mAnimationSourceViewProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabListEditorCoordinator.TabListEditorController mTabListEditorController;
    @Mock private TabGroupTitleEditor mTabGroupTitleEditor;
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
    @Mock private MessagingBackendService mMessagingBackendService;
    @Mock private SharedImageTilesCoordinator mSharedImageTilesCoordinator;
    @Mock private DesktopWindowStateProvider mDesktopWindowStateProvider;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor private ArgumentCaptor<TabGroupModelFilterObserver> mTabGroupModelFilterObserverCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mCollaborationActivityMessageCardCaptor;
    @Captor private ArgumentCaptor<Callback<GroupDataOrFailureOutcome>> mReadGroupCallbackCaptor;
    @Captor private ArgumentCaptor<DataSharingService.Observer> mSharingObserverCaptor;
    @Captor private ArgumentCaptor<PropertyModel> mMessageCardModelCaptor;

    @Captor
    private ArgumentCaptor<MessagingBackendService.PersistentMessageObserver>
            mPersistentMessageObserverCaptor;

    private final ObservableSupplierImpl<TabModelFilter> mCurrentTabModelFilterSupplier =
            new ObservableSupplierImpl<>();

    private UserActionTester mActionTester;
    private Tab mTab1;
    private Tab mTab2;
    private Activity mActivity;
    private PropertyModel mModel;
    private TabGridDialogMediator mMediator;
    private SharedGroupTestHelper mSharedGroupTestHelper;

    @Before
    public void setUp() {
        mActionTester = new UserActionTester();

        mJniMocker.mock(TabGroupSyncFeaturesJni.TEST_HOOKS, mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mProfile.isNativeInitialized()).thenReturn(true);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        DataSharingServiceFactory.setForTesting(mDataSharingService);
        MessagingBackendServiceFactory.setForTesting(mMessagingBackendService);
        mockPersistentMessages(/* added= */ 1, /* navigated= */ 2, /* removed= */ 3);
        mSharedGroupTestHelper =
                new SharedGroupTestHelper(mDataSharingService, mReadGroupCallbackCaptor);

        mTab1 = prepareTab(TAB1_ID, TAB1_TITLE);
        mTab2 = prepareTab(TAB2_ID, TAB2_TITLE);
        List<Tab> tabs1 = new ArrayList<>(Arrays.asList(mTab1));
        List<Tab> tabs2 = new ArrayList<>(Arrays.asList(mTab2));

        mCurrentTabModelFilterSupplier.set(mTabGroupModelFilter);
        doReturn(mProfile).when(mTabModel).getProfile();
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doReturn(POSITION1).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(POSITION2).when(mTabGroupModelFilter).indexOf(mTab2);
        when(mTabGroupModelFilter.isIncognitoBranded()).thenReturn(false);
        doReturn(mTab1).when(mTabGroupModelFilter).getTabAt(POSITION1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(POSITION2);
        doReturn(tabs1).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);
        doReturn(tabs2).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);
        doReturn(POSITION1).when(mTabGroupModelFilter).index();
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
        doReturn(mTabCreator).when(mTabCreatorManager).getTabCreator(anyBoolean());
        doReturn(mEditable).when(mTitleTextView).getText();
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mEditable).toString();
        doReturn(null).when(mRecyclerViewPositionSupplier).get();

        mActivity = Robolectric.buildActivity(TestActivity.class).get();
        mModel = spy(new PropertyModel(TabGridDialogProperties.ALL_KEYS));
        remakeMediator(/* withResetHandler= */ true, /* withAnimSource= */ true);

        mMediator.initWithNative(() -> mTabListEditorController, mTabGroupTitleEditor);
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
        mMediator.onToolbarMenuItemClick(R.id.select_tabs, TAB1_ID, /* collaborationId= */ null);
        verify(mTabListEditorController).configureToolbarWithMenuItems(captor.capture());
        verify(mRecyclerViewPositionSupplier, times(1)).get();
        verify(mTabListEditorController).show(any(), eq(null));
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
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            verify(mDialogController).resetWithListOfTabs(null);
        }

        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            verify(mDialogController, times(2)).resetWithListOfTabs(null);
        } else {
            verify(mDialogController).resetWithListOfTabs(null);
        }
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
        mMediator.onReset(tabGroup);

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
        mMediator.onReset(tabGroup);

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

        // TabGroupTitleEditor should not react to text change when there is no focus.
        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), any(String.class));
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
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

        verify(mTabGroupTitleEditor).storeTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        verify(mTabGroupTitleEditor).updateTabGroupTitle(eq(mTab1), eq(CUSTOMIZED_DIALOG_TITLE));
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

        verify(mTabGroupTitleEditor).storeTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        verify(mTabGroupTitleEditor).updateTabGroupTitle(eq(mTab1), eq(CUSTOMIZED_DIALOG_TITLE));
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

        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(TAB1_TITLE));
    }

    @Test
    public void tabAddition() {
        Tab newTab = prepareTab(TAB3_ID, TAB3_TITLE);
        // Mock that the animation source view is not null, and the dialog is showing.
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mView);
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, true);

        doReturn(true).when(mTabGroupModelFilter).isTabModelRestored();
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
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false);
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
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false);
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
        verify(mTabSwitcherResetHandler, never()).resetWithTabList(mTabGroupModelFilter, false);

        mMediator.onReset(null);
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
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        assertThat(
                mTabGroupTitleEditor.getTabGroupTitle(mTab1.getRootId()),
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
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB3_ID);

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
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

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
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false);
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
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        // Mock that tab1 and tab2 are in the same group, and we are undoing tab2.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);
        mTabModelObserverCaptor.getValue().tabClosureUndone(mTab2);

        // If current group has a stored title, dialog title should be set to stored title when
        // undoing a closure.
        assertThat(
                mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(CUSTOMIZED_DIALOG_TITLE));
        verify(mTabSwitcherResetHandler).resetWithTabList(mTabGroupModelFilter, false);
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
        verify(mTabSwitcherResetHandler, never()).resetWithTabList(mTabGroupModelFilter, false);
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

        mMediator.initWithNative(() -> mTabListEditorController, mTabGroupTitleEditor);
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

        verify(mTabGroupTitleEditor).storeTabGroupTitle(eq(TAB1_ID), eq(CUSTOMIZED_DIALOG_TITLE));
        verify(mTabGroupTitleEditor).updateTabGroupTitle(eq(mTab1), eq(CUSTOMIZED_DIALOG_TITLE));
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
        verify(mTabGroupTitleEditor).deleteTabGroupTitle(eq(TAB1_ID));
        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(DIALOG_TITLE2));
        verify(mTabGroupTitleEditor).updateTabGroupTitle(eq(mTab1), eq(DIALOG_TITLE2));
    }

    @Test
    public void hideDialog_NoModifiedGroupTitle() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        mModel.set(TabGridDialogProperties.HEADER_TITLE, TAB1_TITLE);

        // Mock that tab1 is in a group.
        createTabGroup(new ArrayList<>(Arrays.asList(mTab1, mTab2)), TAB1_ID, TAB_GROUP_ID);

        mMediator.hideDialog(false);

        // When title is not updated, don't store title when hide dialog.
        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
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
        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
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
        verify(mTabGroupTitleEditor, never()).storeTabGroupTitle(anyInt(), anyString());
        verify(mTabGroupTitleEditor, never()).updateTabGroupTitle(any(Tab.class), anyString());
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

        mMediator.onReset(null);

        assertFalse(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE));

        // Simulate the animation finishing.
        mModel.get(TabGridDialogProperties.VISIBILITY_LISTENER).finishedHidingDialogView();
        verify(mDialogController).postHiding();
    }

    @Test
    public void onReset_DialogNotVisible_NoOp() {
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);

        mMediator.onReset(null);

        verifyNoMoreInteractions(mDialogController);
    }

    @Test
    public void finishedHiding() {
        mMediator.finishedHidingDialogView();

        verify(mDialogController).resetWithListOfTabs(null);
        verify(mDialogController).postHiding();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void showDialog_FromGts_setSelectedColor() {
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        // Mock that we have a stored color stored with reference to root ID of tab1.
        // when(mTabGroupModelFilter.getTabGroupColor(mTab1.getRootId())).thenReturn(COLOR_2);
        mModel.set(TabGridDialogProperties.TAB_GROUP_COLOR_ID, COLOR_2);

        mMediator.onReset(tabGroup);
        mMediator.setSelectedTabGroupColor(COLOR_3);

        // Assert that the color has changed both in the property model and the model filter.
        assertThat(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), equalTo(COLOR_3));
        verify(mTabGroupModelFilter).setTabGroupColor(mTab1.getRootId(), COLOR_3);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
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
        mMediator.onReset(tabGroup);

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
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.FORCE_LIST_TAB_SWITCHER
    })
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
        mMediator.onReset(tabGroup);

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
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        mMediator.onReset(tabGroup);

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
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void showDialog_FromStrip() {
        // For strip we don't play zoom-in/zoom-out for show/hide dialog, and thus
        // the animationParamsProvider is null.
        remakeMediator(/* withResetHandler= */ true, /* withAnimSource= */ false);
        mMediator.initWithNative(() -> mTabListEditorController, mTabGroupTitleEditor);

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
        mMediator.onReset(tabGroup);

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
        mMediator.initWithNative(() -> mTabListEditorController, mTabGroupTitleEditor);
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
        doReturn(CUSTOMIZED_DIALOG_TITLE).when(mTabGroupTitleEditor).getTabGroupTitle(TAB1_ID);

        mMediator.onReset(tabGroup);

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
        mMediator.initWithNative(() -> mTabListEditorController, mTabGroupTitleEditor);
        // Mock that the dialog is hidden and animation source view is set to some mock view for
        // testing purpose.
        mModel.set(TabGridDialogProperties.IS_DIALOG_VISIBLE, false);
        mModel.set(TabGridDialogProperties.ANIMATION_SOURCE_VIEW, mock(View.class));
        // Mock that tab1 and tab2 are in a group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        mMediator.onReset(tabGroup);

        assertThat(mModel.get(TabGridDialogProperties.IS_DIALOG_VISIBLE), equalTo(true));
        // Animation source view should be set to null so that dialog will setup basic animation.
        assertThat(mModel.get(TabGridDialogProperties.ANIMATION_SOURCE_VIEW), equalTo(null));
    }

    @Test
    public void testDialogToolbarMenu_SelectionModeV2() {
        // Mock that currently the title text is focused and the keyboard is showing. The current
        // tab is tab1 which is in a group of {tab1, tab2}.
        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, true);
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        mMediator.onToolbarMenuItemClick(R.id.select_tabs, TAB1_ID, /* collaborationId= */ null);

        assertThat(mModel.get(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED), equalTo(false));
        verify(mRecyclerViewPositionSupplier, times(1)).get();
        verify(mTabListEditorController).show(eq(tabGroup), eq(null));
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.SelectTabs"));
    }

    @Test
    public void testDialogToolbarMenu_EditGroupName() {
        mModel.set(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED, false);

        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        mMediator.onToolbarMenuItemClick(
                R.id.edit_group_name, TAB1_ID, /* collaborationId= */ null);
        assertTrue(mModel.get(TabGridDialogProperties.IS_TITLE_TEXT_FOCUSED));
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.Rename"));
    }

    @Test
    public void testDialogToolbarMenu_EditGroupColor() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        mMediator.onToolbarMenuItemClick(
                R.id.edit_group_color, TAB1_ID, /* collaborationId= */ null);
        verify(mShowColorPickerPopupRunnable).run();
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.EditColor"));
    }

    @Test
    public void testDialogToolbarMenu_CloseGroup() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);
        when(mTabGroupModelFilter.isIncognitoBranded()).thenReturn(true);

        mMediator.onToolbarMenuItemClick(R.id.close_tab, TAB1_ID, /* collaborationId= */ null);
        verify(mTabGroupModelFilter)
                .closeTabs(TabClosureParams.closeTabs(tabGroup).hideTabGroups(true).build());

        verifyNoInteractions(mActionConfirmationManager);
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.Close"));
    }

    @Test
    public void testDialogToolbarMenu_DeleteGroup() {
        mMediator.setCurrentTabIdForTesting(TAB1_ID);
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);
        when(mTabGroupModelFilter.isIncognitoBranded()).thenReturn(true);

        mMediator.onToolbarMenuItemClick(R.id.delete_tab, TAB1_ID, /* collaborationId= */ null);
        verify(mTabGroupModelFilter).closeTabs(TabClosureParams.closeTabs(tabGroup).build());
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.Delete"));

        when(mTabGroupModelFilter.isIncognitoBranded()).thenReturn(false);
        mMediator.onToolbarMenuItemClick(R.id.delete_tab, TAB1_ID, /* collaborationId= */ null);
        verify(mActionConfirmationManager).processDeleteGroupAttempt(any());
        assertEquals(2, mActionTester.getActionCount("TabGridDialogMenu.Delete"));
    }

    @Test
    public void testDialogToolbarMenu_ManageSharing() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        mMediator.onToolbarMenuItemClick(R.id.manage_sharing, TAB1_ID, COLLABORATION_ID1);
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.ManageSharing"));
        verify(mDataSharingTabManager).showManageSharing(any(), eq(COLLABORATION_ID1));
    }

    @Test
    public void testDialogToolbarMenu_RecentActivity() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        mMediator.onToolbarMenuItemClick(R.id.recent_activity, TAB1_ID, COLLABORATION_ID1);
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.RecentActivity"));
        verify(mDataSharingTabManager).showRecentActivity(COLLABORATION_ID1);
    }

    @Test
    public void testDialogToolbarMenu_DeleteSharedGroup() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        mMediator.onToolbarMenuItemClick(R.id.delete_shared_group, TAB1_ID, COLLABORATION_ID1);
        verify(mActionConfirmationManager).processDeleteSharedGroupAttempt(eq(GROUP_TITLE), any());
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.DeleteShared"));
    }

    @Test
    public void testDialogToolbarMenu_LeaveSharedGroup() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1, GROUP_MEMBER2);

        CoreAccountInfo coreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId(EMAIL2, GAIA_ID2);
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(coreAccountInfo);

        mMediator.onToolbarMenuItemClick(R.id.leave_group, TAB1_ID, COLLABORATION_ID1);
        verify(mActionConfirmationManager).processLeaveGroupAttempt(eq(GROUP_TITLE), any());
        assertEquals(1, mActionTester.getActionCount("TabGridDialogMenu.LeaveShared"));
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

        // Mock that mTab2 is the current tab for the dialog.
        doReturn(0).when(mTabGroupModelFilter).indexOf(mTab1);
        doReturn(mTab2).when(mTabGroupModelFilter).getTabAt(0);
        doReturn(tabGroup).when(mTabGroupModelFilter).getRelatedTabList(TAB2_ID);

        // Reset and confirm scroll index.
        mMediator.onReset(tabGroup);

        assertEquals(1, mModel.get(TabGridDialogProperties.INITIAL_SCROLL_INDEX).intValue());
    }

    @Test
    public void testTabUngroupBarText() {
        // Mock that tab1 and tab2 are in the same group.
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, TAB1_ID, TAB_GROUP_ID);

        mMediator.onReset(tabGroup);
        // Check that the text indicates that this is not the last tab in the group.
        assertEquals(
                mActivity.getString(R.string.tab_grid_dialog_remove_from_group),
                mModel.get(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT));

        // Mock that tab1 is the only tab that remains in the group.
        List<Tab> tabGroupAfterUngroup = new ArrayList<>(Arrays.asList(mTab1));
        doReturn(tabGroupAfterUngroup).when(mTabGroupModelFilter).getRelatedTabList(TAB1_ID);

        mMediator.onReset(tabGroupAfterUngroup);
        // Check that the text indicates that this is the last tab in the group.
        assertEquals(
                mActivity.getString(R.string.remove_last_tab_action),
                mModel.get(TabGridDialogProperties.DIALOG_UNGROUP_BAR_TEXT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testTabGroupColorUpdated() {
        int rootId = TAB1_ID;
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, rootId, TAB_GROUP_ID);

        mMediator.onReset(tabGroup);

        assertNotEquals(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), COLOR_3);

        mTabGroupModelFilterObserverCaptor.getValue().didChangeTabGroupColor(rootId, COLOR_3);

        assertThat(mModel.get(TabGridDialogProperties.TAB_GROUP_COLOR_ID), equalTo(COLOR_3));
    }

    @Test
    public void testTabGroupTitleUpdated() {
        int rootId = TAB1_ID;
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, rootId, TAB_GROUP_ID);

        when(mTabGroupTitleEditor.getTabGroupTitle(rootId)).thenReturn(CUSTOMIZED_DIALOG_TITLE);
        mMediator.onReset(tabGroup);

        assertEquals(mModel.get(TabGridDialogProperties.HEADER_TITLE), CUSTOMIZED_DIALOG_TITLE);

        String newTitle = "BAR";
        when(mTabGroupTitleEditor.getTabGroupTitle(rootId)).thenReturn(newTitle);
        mTabGroupModelFilterObserverCaptor.getValue().didChangeTabGroupTitle(rootId, newTitle);

        assertThat(mModel.get(TabGridDialogProperties.HEADER_TITLE), equalTo(newTitle));
    }

    @Test
    public void destroy() {
        mMediator.destroy();

        verify(mTabGroupModelFilter).removeObserver(mTabModelObserverCaptor.capture());
        assertFalse(mCurrentTabModelFilterSupplier.hasObservers());
        verify(mDesktopWindowStateProvider).removeObserver(mMediator);
        verify(mMessagingBackendService).removePersistentMessageObserver(any());
    }

    @Test
    public void testUpdateShareData_Incognito() {
        reset(mSharedImageTilesCoordinator);
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));

        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);

        when(mTabGroupModelFilter.isIncognitoBranded()).thenReturn(true);
        resetForDataSharing(/* isShared= */ false);

        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        verify(mSharedImageTilesCoordinator).updateCollaborationId(null);
    }

    @Test
    public void testShowOrUpdateCollaborationActivityMessageCard() {
        reset(mSharedImageTilesCoordinator);
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));

        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(true);
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1, GROUP_MEMBER2);
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertTrue(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        verify(mSharedImageTilesCoordinator).updateCollaborationId(COLLABORATION_ID1);
        verify(mDialogController, never()).addMessageCardItem(/* position= */ eq(0), any());

        // Reset with null first as re-using the same TabGroupId does not reset the observer.
        mMediator.onReset(null);
        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(false);
        resetForDataSharing(/* isShared= */ false);
        verify(mDialogController).removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
        verify(mSharedImageTilesCoordinator).updateCollaborationId(null);
        assertTrue(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));

        // Reset with null first as re-using the same TabGroupId does not reset the observer.
        mMediator.onReset(null);
        when(mDialogController.messageCardExists(MessageType.COLLABORATION_ACTIVITY))
                .thenReturn(false);
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        verify(mDialogController)
                .addMessageCardItem(/* position= */ eq(0), mMessageCardModelCaptor.capture());
        assertTrue(mModel.get(TabGridDialogProperties.SHOW_SHARE_BUTTON));
        assertFalse(mModel.get(TabGridDialogProperties.SHOW_IMAGE_TILES));
        String text = mMessageCardModelCaptor.getValue().get(DESCRIPTION_TEXT).toString();
        assertTrue(text, text.contains("3"));

        reset(mDialogController);
        mockPersistentMessages(/* added= */ 1, /* navigated= */ 2, /* removed= */ 4);
        verify(mMessagingBackendService)
                .addPersistentMessageObserver(mPersistentMessageObserverCaptor.capture());
        mPersistentMessageObserverCaptor
                .getValue()
                .displayPersistentMessage(makePersistentMessage(UserAction.TAB_REMOVED));
        verify(mDialogController)
                .addMessageCardItem(/* position= */ eq(0), mMessageCardModelCaptor.capture());
        text = mMessageCardModelCaptor.getValue().get(DESCRIPTION_TEXT).toString();
        assertFalse(text, text.contains("3"));
        assertTrue(text, text.contains("4"));

        reset(mDialogController);
        mockPersistentMessages(/* added= */ 0, /* navigated= */ 2, /* removed= */ 4);
        mPersistentMessageObserverCaptor
                .getValue()
                .hidePersistentMessage(makePersistentMessage(UserAction.TAB_ADDED));
        verify(mDialogController)
                .addMessageCardItem(/* position= */ eq(0), mMessageCardModelCaptor.capture());
        text = mMessageCardModelCaptor.getValue().get(DESCRIPTION_TEXT).toString();
        assertFalse(text, text.contains("1"));
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

        verify(mDataSharingTabManager, never()).showRecentActivity(any());
        verify(mDialogController, atLeastOnce())
                .removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
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

        verify(mDataSharingTabManager).showRecentActivity(COLLABORATION_ID1);
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
                        mCurrentTabModelFilterSupplier,
                        mTabCreatorManager,
                        withResetHandler ? mTabSwitcherResetHandler : null,
                        mRecyclerViewPositionSupplier,
                        withAnimSource ? mAnimationSourceViewProvider : null,
                        mSnackbarManager,
                        mSharedImageTilesCoordinator,
                        mDataSharingTabManager,
                        /* componentName= */ "",
                        mShowColorPickerPopupRunnable,
                        mActionConfirmationManager,
                        mModalDialogManager,
                        mDesktopWindowStateProvider);
    }

    @Test
    public void onReset_NullAfterSharedGroup() {
        resetForDataSharing(/* isShared= */ true, GROUP_MEMBER1);
        mMediator.onReset(null);

        verify(mDialogController).removeMessageCardItem(MessageType.COLLABORATION_ACTIVITY);
    }

    @Test
    public void onAppHeaderStateChange_setAppHeaderHeight() {
        // App header height not set.
        assertThat(mModel.get(TabGridDialogProperties.APP_HEADER_HEIGHT), equalTo(0));
        // Rect with height = 10.
        Rect headerRect = new Rect(0, 0, 10, 10);
        AppHeaderState state = new AppHeaderState(headerRect, headerRect, true);
        when(mDesktopWindowStateProvider.getAppHeaderState()).thenReturn(state);

        mMediator.onAppHeaderStateChanged(state);

        assertThat(mModel.get(TabGridDialogProperties.APP_HEADER_HEIGHT), equalTo(10));
    }

    private void resetForDataSharing(boolean isShared, GroupMember... members) {
        int rootId = TAB1_ID;
        List<Tab> tabGroup = new ArrayList<>(Arrays.asList(mTab1, mTab2));
        createTabGroup(tabGroup, rootId, TAB_GROUP_ID);

        setupSyncedGroup(isShared);

        mMediator.onReset(tabGroup);

        if (isShared) {
            mSharedGroupTestHelper.respondToReadGroup(COLLABORATION_ID1, members);
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

    private void createTabGroup(List<Tab> tabs, int rootId, @Nullable Token tabGroupId) {
        for (Tab tab : tabs) {
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabs);
            when(mTabGroupModelFilter.getRelatedTabListForRootId(rootId)).thenReturn(tabs);
            when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
            when(tab.getRootId()).thenReturn(rootId);
            when(tab.getTabGroupId()).thenReturn(tabGroupId);
        }
    }

    private PersistentMessage makePersistentMessage(@UserAction int action) {
        MessageAttribution attribution = new MessageAttribution();
        attribution.localTabId = TAB1_ID;
        attribution.localTabGroupId = new LocalTabGroupId(TAB_GROUP_ID);
        PersistentMessage message = new PersistentMessage();
        message.attribution = attribution;
        message.action = action;
        return message;
    }

    private void mockPersistentMessages(int added, int navigated, int removed) {
        List<PersistentMessage> messageList = new ArrayList<>();
        for (int i = 0; i < added; i++) {
            messageList.add(makePersistentMessage(UserAction.TAB_ADDED));
        }
        for (int i = 0; i < navigated; i++) {
            messageList.add(makePersistentMessage(UserAction.TAB_NAVIGATED));
        }
        for (int i = 0; i < removed; i++) {
            messageList.add(makePersistentMessage(UserAction.TAB_REMOVED));
        }
        when(mMessagingBackendService.getMessagesForGroup(any(), any())).thenReturn(messageList);
    }
}
