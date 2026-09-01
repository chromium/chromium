// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid.ImmersivePlaybackConfirmationCallback;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.ImmersivePlaybackConfirmationStatus;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Tests for {@link ImmersivePlaybackMessageController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ImmersivePlaybackMessageControllerTest {
    @Mock private MessageDispatcher mMessageDispatcher;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Tab mTab;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private ImmersivePlaybackConfirmationCallback mCallback;
    @Mock private WebContents mWebContents;

    private Context mContext;
    private ImmersivePlaybackMessageController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mContext = Robolectric.buildActivity(Activity.class).create().get();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
        mController =
                new ImmersivePlaybackMessageController(
                        mContext,
                        () -> mMessageDispatcher,
                        () -> mModalDialogManager,
                        mTab,
                        mFullscreenManager);
    }

    @Test
    public void testShow_RegistersObservers() {
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(PropertyModel.class),
                        any(),
                        eq(MessageScopeType.NAVIGATION),
                        eq(false));
        verify(mTab).addObserver(any());
        verify(mFullscreenManager).addObserver(any());
    }

    @Test
    public void testShow_EnqueuesMessageWithIcon() {
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher)
                .enqueueMessage(
                        messageCaptor.capture(), any(), eq(MessageScopeType.NAVIGATION), eq(false));
        PropertyModel messageModel = messageCaptor.getValue();
        Assert.assertEquals(
                R.drawable.ic_panorama_horizontal_24dp,
                messageModel.get(MessageBannerProperties.ICON_RESOURCE_ID));
    }

    @Test
    public void testActionClicks_OpensDialog() {
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);

        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher)
                .enqueueMessage(
                        messageCaptor.capture(), any(), eq(MessageScopeType.NAVIGATION), eq(false));
        PropertyModel messageModel = messageCaptor.getValue();

        Supplier<Integer> primaryAction =
                messageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION);
        int behavior = primaryAction.get();

        verify(mTab).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mModalDialogManager)
                .showDialog(
                        any(PropertyModel.class),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));
        Assert.assertEquals(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY, behavior);
    }

    @Test
    public void testDialogSelection_ConfirmsOption() {
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);

        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher)
                .enqueueMessage(
                        messageCaptor.capture(), any(), eq(MessageScopeType.NAVIGATION), eq(false));
        PropertyModel messageModel = messageCaptor.getValue();

        var unused = messageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();

        ArgumentCaptor<PropertyModel> dialogCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager)
                .showDialog(
                        dialogCaptor.capture(),
                        eq(ModalDialogManager.ModalDialogType.APP),
                        eq(true));

        PropertyModel dialogModel = dialogCaptor.getValue();
        ImmersiveVideoFormatRadioGroup radioGroup =
                (ImmersiveVideoFormatRadioGroup) dialogModel.get(ModalDialogProperties.CUSTOM_VIEW);
        Assert.assertNotNull(radioGroup);

        radioGroup.checkOption(
                ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveProjectionType.HEMISPHERE);

        dialogModel
                .get(ModalDialogProperties.CONTROLLER)
                .onDismiss(dialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);

        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CONFIRMED,
                        ImmersiveStereoMode.SIDE_BY_SIDE,
                        ImmersiveProjectionType.HEMISPHERE,
                        false);
    }

    @Test
    public void testActionClicks_RecommendedOptions_ConfirmsImmediately() {
        mController.show(
                mCallback, ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveProjectionType.HEMISPHERE);

        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher)
                .enqueueMessage(
                        messageCaptor.capture(), any(), eq(MessageScopeType.NAVIGATION), eq(false));
        PropertyModel messageModel = messageCaptor.getValue();

        Supplier<Integer> primaryAction =
                messageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION);
        int behavior = primaryAction.get();

        verify(mTab).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mModalDialogManager, never()).showDialog(any(), anyInt(), anyBoolean());
        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.CONFIRMED,
                        ImmersiveStereoMode.SIDE_BY_SIDE,
                        ImmersiveProjectionType.HEMISPHERE,
                        true);
        Assert.assertEquals(PrimaryActionClickBehavior.DISMISS_IMMEDIATELY, behavior);
    }

    @Test
    public void testDismissNoAction_Declines() {
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);

        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher)
                .enqueueMessage(
                        messageCaptor.capture(), any(), eq(MessageScopeType.NAVIGATION), eq(false));
        PropertyModel messageModel = messageCaptor.getValue();

        Callback<Integer> dismissCallback = messageModel.get(MessageBannerProperties.ON_DISMISSED);
        dismissCallback.onResult(DismissReason.TIMER);

        verify(mTab).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.DECLINED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD,
                        false);
    }

    @Test
    public void testDismiss_DismissesMessage() {
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        clearInvocations(mMessageDispatcher);
        mController.dismiss();
        verify(mMessageDispatcher)
                .dismissMessage(any(PropertyModel.class), eq(DismissReason.DISMISSED_BY_FEATURE));
    }

    @Test
    public void testDismiss_DismissesDialog() {
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        ArgumentCaptor<PropertyModel> messageCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mMessageDispatcher)
                .enqueueMessage(
                        messageCaptor.capture(), any(), eq(MessageScopeType.NAVIGATION), eq(false));
        PropertyModel messageModel = messageCaptor.getValue();
        Supplier<Integer> primaryAction =
                messageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION);

        // Consume message, show dialog
        int unused = primaryAction.get();
        mController.dismiss();
        verify(mModalDialogManager).dismissDialog(any(), anyInt());
    }

    @Test
    public void testShow_NullModalDialogManager_FailsImmediately() {
        mController =
                new ImmersivePlaybackMessageController(
                        mContext,
                        () -> mMessageDispatcher,
                        SupplierUtils.ofNull(),
                        mTab,
                        mFullscreenManager);

        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);

        verify(mCallback)
                .onResult(
                        ImmersivePlaybackConfirmationStatus.FAILED,
                        ImmersiveStereoMode.MONO,
                        ImmersiveProjectionType.QUAD,
                        false);
        verify(mMessageDispatcher, never()).enqueueMessage(any(), any(), anyInt(), anyBoolean());
        verify(mTab, never()).addObserver(any());
    }

    @Test
    public void testShow_DuplicateCalls_GuardAddsObserversOnce() {
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        verify(mTab).addObserver(any());
        verify(mFullscreenManager).addObserver(any());

        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);

        // Consecutive call to show() will unregister and re-register the observers.
        verify(mTab).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mTab, times(2)).addObserver(any());
        verify(mFullscreenManager, times(2)).addObserver(any());
    }

    @Test
    public void testObservers_TriggerEvents_DismissesMessage() {
        ArgumentCaptor<TabObserver> tabCaptor = ArgumentCaptor.forClass(TabObserver.class);
        ArgumentCaptor<FullscreenManager.Observer> fsCaptor =
                ArgumentCaptor.forClass(FullscreenManager.Observer.class);

        // 1. Test page load started
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        verify(mTab).addObserver(tabCaptor.capture());
        clearInvocations(mMessageDispatcher);
        tabCaptor.getValue().onPageLoadStarted(mTab, null);
        verify(mMessageDispatcher)
                .dismissMessage(any(PropertyModel.class), eq(DismissReason.DISMISSED_BY_FEATURE));

        // 2. Test content changed
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        clearInvocations(mMessageDispatcher);
        tabCaptor.getValue().onContentChanged(mTab);
        verify(mMessageDispatcher)
                .dismissMessage(any(PropertyModel.class), eq(DismissReason.DISMISSED_BY_FEATURE));

        // 3. Test exit fullscreen
        mController.show(mCallback, ImmersiveStereoMode.MONO, ImmersiveProjectionType.QUAD);
        verify(mFullscreenManager, times(3)).addObserver(fsCaptor.capture());
        clearInvocations(mMessageDispatcher);
        fsCaptor.getValue().onExitFullscreen(mTab);
        verify(mMessageDispatcher)
                .dismissMessage(any(PropertyModel.class), eq(DismissReason.DISMISSED_BY_FEATURE));
    }
}
