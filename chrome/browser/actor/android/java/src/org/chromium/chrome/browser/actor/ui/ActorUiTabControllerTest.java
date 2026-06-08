// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;

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
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.StoppedReason;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.ActorOverlayState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.HandoffButtonState;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ActorUiTabController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.GLIC)
public class ActorUiTabControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Profile mProfile;
    @Mock private ActorKeyedService mActorKeyedService;
    @Mock private Callback<Boolean> mCallback;

    @Captor private ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    private final UserDataHost mUserDataHost = new UserDataHost();
    private Activity mActivity;
    private ActorUiTabController mController;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(mWindowAndroid).when(mTab).getWindowAndroid();
        doReturn(mModalDialogManager).when(mWindowAndroid).getModalDialogManager();
        doReturn(mActivity).when(mTab).getContext();
        doReturn(mProfile).when(mTab).getProfile();
        ActorKeyedServiceFactory.setForTesting(mActorKeyedService);

        mController = ActorUiTabController.from(mTab);
    }

    @Test
    public void testShowTaskAbortConfirmationDialog_actorInactive() {
        // Actor is inactive by default.
        assertFalse(mController.showTaskAbortConfirmationDialog(mCallback));
        verify(mModalDialogManager, never()).showDialog(any(), anyInt());
    }

    @Test
    public void testShowTaskAbortConfirmationDialog_actorActive() {
        int taskId = 123;
        int tabId = 456;
        doReturn(tabId).when(mTab).getId();
        doReturn(taskId).when(mActorKeyedService).getActiveTaskIdOnTab(tabId);

        // Set Actor active.
        ActorOverlayState activeOverlayState = new ActorOverlayState(true, false, false);
        HandoffButtonState handoffState = new HandoffButtonState(false, 0);
        UiTabState activeState = new UiTabState(1, activeOverlayState, handoffState, 0, false);
        mController.onUiTabStateChange(activeState);

        assertTrue(mController.showTaskAbortConfirmationDialog(mCallback));
        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mPropertyModelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Simulate positive button click (leave site)
        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);

        // Verify Glic task was stopped.
        verify(mActorKeyedService).stopTask(taskId, StoppedReason.USER_NAVIGATED_AWAY);

        // Verify callback was executed with true.
        verify(mCallback).onResult(true);
    }

    @Test
    public void testShowTaskAbortConfirmationDialog_callback_cancelled() {
        int taskId = 123;
        int tabId = 456;
        doReturn(tabId).when(mTab).getId();
        doReturn(taskId).when(mActorKeyedService).getActiveTaskIdOnTab(tabId);

        // Set Actor active.
        ActorOverlayState activeOverlayState = new ActorOverlayState(true, false, false);
        HandoffButtonState handoffState = new HandoffButtonState(false, 0);
        UiTabState activeState = new UiTabState(1, activeOverlayState, handoffState, 0, false);
        mController.onUiTabStateChange(activeState);

        assertTrue(mController.showTaskAbortConfirmationDialog(mCallback));
        verify(mModalDialogManager)
                .showDialog(mPropertyModelCaptor.capture(), eq(ModalDialogType.APP));

        PropertyModel model = mPropertyModelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        // Simulate negative button click (cancel/stay on site)
        controller.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);

        // Verify Glic task was NOT stopped.
        verify(mActorKeyedService, never()).stopTask(anyInt(), anyInt());

        // Verify callback was executed with false.
        verify(mCallback).onResult(false);
    }

    @Test
    public void testStopActorTask() {
        int taskId = 123;
        int tabId = 456;
        doReturn(tabId).when(mTab).getId();
        doReturn(taskId).when(mActorKeyedService).getActiveTaskIdOnTab(tabId);

        mController.stopActorTask();

        verify(mActorKeyedService).stopTask(taskId, StoppedReason.USER_NAVIGATED_AWAY);
    }
}
