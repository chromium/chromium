// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.autofill.PaymentsBubbleClosedReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Tests for {@link MandatoryReauthOptInBottomSheetViewBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class MandatoryReauthOptInBottomSheetModuleTest {
    private MandatoryReauthOptInBottomSheetViewBridge mViewBridge;
    ArgumentCaptor<BottomSheetContent> mContentCaptor =
            ArgumentCaptor.forClass(BottomSheetContent.class);
    private final ArgumentCaptor<BottomSheetObserver> mObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private BottomSheetController mController;
    @Mock private MandatoryReauthOptInBottomSheetControllerBridge.Natives mControllerBridgeJniMock;

    private static final long sPlaceholderNativePointer = 1;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mJniMocker.mock(
                MandatoryReauthOptInBottomSheetControllerBridgeJni.TEST_HOOKS,
                mControllerBridgeJniMock);
        setUpBottomSheetController();
        mViewBridge =
                new MandatoryReauthOptInBottomSheetViewBridge(
                        new MandatoryReauthOptInBottomSheetCoordinator(
                                Robolectric.buildActivity(Activity.class).get(),
                                mController,
                                new MandatoryReauthOptInBottomSheetControllerBridge(
                                        sPlaceholderNativePointer)));
    }

    private void setUpBottomSheetController() {
        when(mController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        doNothing().when(mController).addObserver(mObserverCaptor.capture());
    }

    @Test
    public void testShowsBottomSheet() {
        mViewBridge.show();

        verify(mController).requestShowContent(any(), anyBoolean());
        verify(mController).addObserver(any());
    }

    @Test
    public void testBottomSheetContents() {
        mViewBridge.show();

        verify(mController).requestShowContent(mContentCaptor.capture(), anyBoolean());

        TextView titleView = (TextView) getView(R.id.mandatory_reauth_opt_in_title);
        TextView explanationView = (TextView) getView(R.id.mandatory_reauth_opt_in_explanation);

        // Check that the custom view contains the expected title and explanation.
        assertThat(titleView.getText(), is("Turn on manual verification?"));
        assertThat(
                explanationView.getText(),
                is(
                        "If you share this device, Chromium can ask you to verify every time you"
                                + " pay using autofill"));

        Button acceptButton = (Button) getView(R.id.mandatory_reauth_opt_in_accept_button);
        Button cancelButton = (Button) getView(R.id.mandatory_reauth_opt_in_cancel_button);

        // Check that the accept/cancel buttons are correctly shown.
        assertThat(acceptButton.getText(), is("Turn on"));
        assertThat(cancelButton.getText(), is("No thanks"));
    }

    @Test
    public void testPromptAcceptedByUser() {
        mViewBridge.show();

        verify(mController).requestShowContent(mContentCaptor.capture(), anyBoolean());

        Button acceptButton = (Button) getView(R.id.mandatory_reauth_opt_in_accept_button);
        acceptButton.performClick();

        verify(mController).hideContent(any(), anyBoolean(), anyInt());
        // verify(mController).removeObserver(mObserverCaptor.getValue());
        // Verify that when the accept button is clicked, user acceptance is relayed via the
        // delegate.
        verify(mControllerBridgeJniMock)
                .onClosed(sPlaceholderNativePointer, PaymentsBubbleClosedReason.ACCEPTED);
    }

    @Test
    public void testPromptCancelledByUser() {
        mViewBridge.show();

        verify(mController).requestShowContent(mContentCaptor.capture(), anyBoolean());

        Button cancelButton = (Button) getView(R.id.mandatory_reauth_opt_in_cancel_button);
        cancelButton.performClick();

        verify(mController).hideContent(any(), anyBoolean(), anyInt());
        // verify(mController).removeObserver(mObserverCaptor.getValue());
        // Verify that when the cancel button is clicked, user cancellation is relayed via the
        // delegate.
        verify(mControllerBridgeJniMock)
                .onClosed(sPlaceholderNativePointer, PaymentsBubbleClosedReason.CANCELLED);
    }

    @Test
    public void testPromptClosedByUser() {
        mViewBridge.show();

        mObserverCaptor.getValue().onSheetClosed(StateChangeReason.SWIPE);

        verify(mController).removeObserver(mObserverCaptor.getValue());
        // Verify that when the bottom sheet is closed without explicit user selection, the close
        // event is relayed via the delegate.
        verify(mControllerBridgeJniMock)
                .onClosed(sPlaceholderNativePointer, PaymentsBubbleClosedReason.CLOSED);
    }

    @Test
    public void testPromptClosedWithoutInteraction() {
        mViewBridge.show();

        mObserverCaptor.getValue().onSheetClosed(StateChangeReason.NONE);

        verify(mController).removeObserver(mObserverCaptor.getValue());
        // Verify that when the bottom sheet is closed without user interaction, the close
        // event is relayed via the delegate.
        verify(mControllerBridgeJniMock)
                .onClosed(sPlaceholderNativePointer, PaymentsBubbleClosedReason.NOT_INTERACTED);
    }

    private View getView(int viewId) {
        View view = mContentCaptor.getValue().getContentView();
        assertNotNull(view);

        return view.findViewById(viewId);
    }
}
