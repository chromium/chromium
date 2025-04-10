// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager.MaybeBlockingResult;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;

/** Unit tests for {@link ActionConfirmationManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionConfirmationManagerUnitTest {
    private static final String TEST_EMAIL = "test@gmail.com";
    private static final String GROUP_TITLE = "Group1";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Profile mProfile;
    @Mock private Activity mActivity;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Callback<@ActionConfirmationResult Integer> mOnResult;
    @Mock private Callback<MaybeBlockingResult> mOnMaybeBlockingResult;
    @Mock private SyncService mSyncService;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;
    @Mock private CoreAccountInfo mCoreAccountInfo;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelArgumentCaptor;
    @Captor private ArgumentCaptor<MaybeBlockingResult> mMaybeBlockingResultCaptor;

    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mActionTester = new UserActionTester();

        UserPrefsJni.setInstanceForTesting(mUserPrefsJni);

        SyncServiceFactory.setInstanceForTesting(mSyncService);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(mCoreAccountInfo);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    @Test
    public void testProcessDeleteGroupAttempt_Positive() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processDeleteGroupAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.POSITIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
    }

    @Test
    public void testProcessUngroupAttempt_Positive() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.POSITIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
    }

    @Test
    public void testProcessUngroupTabAttempt_Positive() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.POSITIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
    }

    @Test
    public void testProcessUngroupTabAttempt_Negative() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.NEGATIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_NEGATIVE);
    }

    @Test
    public void testProcessUngroupTabAttempt_PrefSet() {
        when(mPrefService.getBoolean(anyString())).thenReturn(true);
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
        verify(mOnResult).onResult(ActionConfirmationResult.IMMEDIATE_CONTINUE);
    }

    @Test
    public void testProcessUngroupTabAttempt_CheckBoxOnPositive() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        View customView =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CUSTOM_VIEW);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setChecked(true);
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.POSITIVE);

        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
        verify(mPrefService).setBoolean(any(), eq(true));
    }

    @Test
    public void testProcessUngroupTabAttempt_CheckBoxOnNegative() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        View customView =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CUSTOM_VIEW);
        CheckBox stopShowingCheckBox = customView.findViewById(R.id.stop_showing_check_box);
        stopShowingCheckBox.setChecked(true);
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.NEGATIVE);

        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_NEGATIVE);
        verify(mPrefService).setBoolean(any(), eq(true));
    }

    @Test
    public void testProcessUngroupTabAttempt_NoIdentityManager() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());

        View customView =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CUSTOM_VIEW);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        assertEquals(
                "This will permanently delete the group from your device",
                descriptionTextView.getText());
    }

    @Test
    public void testProcessUngroupTabAttempt_NoSignIn() {
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);

        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());

        View customView =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CUSTOM_VIEW);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        assertEquals(
                "This will permanently delete the group from your device",
                descriptionTextView.getText());
    }

    @Test
    public void testProcessUngroupTabAttempt_SignInButNoSync() {
        when(mCoreAccountInfo.getEmail()).thenReturn(TEST_EMAIL);
        when(mSyncService.getActiveDataTypes()).thenReturn(Collections.emptySet());

        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());

        View customView =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CUSTOM_VIEW);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        assertEquals(
                "This will permanently delete the group from your device",
                descriptionTextView.getText());
    }

    @Test
    public void testProcessUngroupTabAttempt_SignInAndSync() {
        when(mCoreAccountInfo.getEmail()).thenReturn(TEST_EMAIL);
        when(mSyncService.getActiveDataTypes())
                .thenReturn(Collections.singleton(DataType.SAVED_TAB_GROUP));

        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processUngroupTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());

        View customView =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CUSTOM_VIEW);
        TextView descriptionTextView = customView.findViewById(R.id.description_text_view);
        assertEquals(
                "This will delete the group from all devices signed into test@gmail.com",
                descriptionTextView.getText());
    }

    @Test
    public void testProcessCloseTabAttempt_Positive() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processCloseTabAttempt(mOnResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.POSITIVE);
        verify(mOnResult).onResult(ActionConfirmationResult.CONFIRMATION_POSITIVE);
    }

    @Test
    public void testProcessDeleteSharedGroupAttempt() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processDeleteSharedGroupAttempt(
                GROUP_TITLE, mOnMaybeBlockingResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.POSITIVE);
        verify(mOnMaybeBlockingResult).onResult(mMaybeBlockingResultCaptor.capture());
        MaybeBlockingResult maybeBlockingResult = mMaybeBlockingResultCaptor.getValue();
        assertEquals(ActionConfirmationResult.CONFIRMATION_POSITIVE, maybeBlockingResult.result);
        assertNotNull(maybeBlockingResult.finishBlocking);
    }

    @Test
    public void testProcessDeleteSharedGroupAttempt_Negative() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processDeleteSharedGroupAttempt(
                GROUP_TITLE, mOnMaybeBlockingResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.NEGATIVE);
        verify(mOnMaybeBlockingResult).onResult(mMaybeBlockingResultCaptor.capture());
        MaybeBlockingResult maybeBlockingResult = mMaybeBlockingResultCaptor.getValue();
        assertEquals(ActionConfirmationResult.CONFIRMATION_NEGATIVE, maybeBlockingResult.result);
        assertNull(maybeBlockingResult.finishBlocking);
    }

    @Test
    public void testProcessLeaveGroupAttempt() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processLeaveGroupAttempt(GROUP_TITLE, mOnMaybeBlockingResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.POSITIVE);
        verify(mOnMaybeBlockingResult).onResult(mMaybeBlockingResultCaptor.capture());
        MaybeBlockingResult maybeBlockingResult = mMaybeBlockingResultCaptor.getValue();
        assertEquals(ActionConfirmationResult.CONFIRMATION_POSITIVE, maybeBlockingResult.result);
        assertNotNull(maybeBlockingResult.finishBlocking);
    }

    @Test
    public void testProcessCollaborationOwnerRemoveLastTabPositive() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processCollaborationOwnerRemoveLastTab(
                GROUP_TITLE, mOnMaybeBlockingResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.POSITIVE);
        verify(mOnMaybeBlockingResult).onResult(mMaybeBlockingResultCaptor.capture());
        MaybeBlockingResult maybeBlockingResult = mMaybeBlockingResultCaptor.getValue();
        assertEquals(ActionConfirmationResult.CONFIRMATION_POSITIVE, maybeBlockingResult.result);
        assertNull(maybeBlockingResult.finishBlocking);

        String action = "TabGroupConfirmation.CollaborationOwnerRemoveLastTab.KeepGroupButton";
        assertTrue(mActionTester.getActions().contains(action));
    }

    @Test
    public void testProcessCollaborationOwnerRemoveLastTab_Negative() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processCollaborationOwnerRemoveLastTab(
                GROUP_TITLE, mOnMaybeBlockingResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onClick(mPropertyModelArgumentCaptor.getValue(), ButtonType.NEGATIVE);
        verify(mOnMaybeBlockingResult).onResult(mMaybeBlockingResultCaptor.capture());
        MaybeBlockingResult maybeBlockingResult = mMaybeBlockingResultCaptor.getValue();
        assertEquals(ActionConfirmationResult.CONFIRMATION_NEGATIVE, maybeBlockingResult.result);
        assertNotNull(maybeBlockingResult.finishBlocking);

        String action = "TabGroupConfirmation.CollaborationOwnerRemoveLastTab.RemoveGroup";
        assertTrue(mActionTester.getActions().contains(action));
    }

    @Test
    public void testProcessCollaborationMemberRemoveLastTab_NoClick() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);
        actionConfirmationManager.processCollaborationMemberRemoveLastTab(
                GROUP_TITLE, mOnMaybeBlockingResult);
        verify(mModalDialogManager).showDialog(mPropertyModelArgumentCaptor.capture(), anyInt());
        Controller controller =
                mPropertyModelArgumentCaptor.getValue().get(ModalDialogProperties.CONTROLLER);
        controller.onDismiss(
                mPropertyModelArgumentCaptor.getValue(), DialogDismissalCause.TOUCH_OUTSIDE);
        verify(mOnMaybeBlockingResult).onResult(mMaybeBlockingResultCaptor.capture());
        MaybeBlockingResult maybeBlockingResult = mMaybeBlockingResultCaptor.getValue();
        assertEquals(ActionConfirmationResult.CONFIRMATION_POSITIVE, maybeBlockingResult.result);
        assertNull(maybeBlockingResult.finishBlocking);

        String action = "TabGroupConfirmation.CollaborationMemberRemoveLastTab.KeepGroupImplicit";
        assertTrue(mActionTester.getActions().contains(action));
    }

    @Test
    public void testWillSkipChecks() {
        ActionConfirmationManager actionConfirmationManager =
                new ActionConfirmationManager(mProfile, mActivity, mModalDialogManager);

        assertFalse(actionConfirmationManager.willSkipCloseTabAttempt());
        when(mPrefService.getBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_CLOSE))
                .thenReturn(true);
        assertTrue(actionConfirmationManager.willSkipCloseTabAttempt());

        assertFalse(actionConfirmationManager.willSkipDeleteGroupAttempt());
        when(mPrefService.getBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_CLOSE))
                .thenReturn(true);
        assertTrue(actionConfirmationManager.willSkipDeleteGroupAttempt());

        assertFalse(actionConfirmationManager.willSkipUngroupTabAttempt());
        when(mPrefService.getBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_TAB_REMOVE))
                .thenReturn(true);
        assertTrue(actionConfirmationManager.willSkipUngroupTabAttempt());

        assertFalse(actionConfirmationManager.willSkipUngroupAttempt());
        when(mPrefService.getBoolean(Pref.STOP_SHOWING_TAB_GROUP_CONFIRMATION_ON_UNGROUP))
                .thenReturn(true);
        assertTrue(actionConfirmationManager.willSkipUngroupAttempt());
    }
}
