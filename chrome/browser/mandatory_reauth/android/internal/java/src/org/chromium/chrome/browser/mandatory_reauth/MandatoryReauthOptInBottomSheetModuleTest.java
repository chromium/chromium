// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/** Tests for {@link MandatoryReauthOptInBottomSheetViewBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class MandatoryReauthOptInBottomSheetModuleTest {
    private MandatoryReauthOptInBottomSheetViewBridge mBridge;
    private MandatoryReauthOptInBottomSheetComponent mComponent;
    private final ArgumentCaptor<BottomSheetObserver> mObserverCaptor =
            ArgumentCaptor.forClass(BottomSheetObserver.class);

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private BottomSheetController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        setUpBottomSheetController();
        mComponent = new MandatoryReauthOptInBottomSheetCoordinator(
                Robolectric.buildActivity(Activity.class).get(), mController);
        mBridge = new MandatoryReauthOptInBottomSheetViewBridge(mComponent);
    }

    private void setUpBottomSheetController() {
        when(mController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        doNothing().when(mController).addObserver(mObserverCaptor.capture());
    }

    @Test
    public void testShowsBottomSheet() {
        mBridge.show();
        verify(mController).requestShowContent(any(), anyBoolean());
        verify(mController).addObserver(any());
    }

    @Test
    public void testClosesBottomSheet() {
        mBridge.show();

        mBridge.close();
        verify(mController).hideContent(any(), anyBoolean());
        verify(mController).removeObserver(mObserverCaptor.getValue());
    }

    @Test
    public void testBottomSheetContents() {
        mBridge.show();

        ArgumentCaptor<BottomSheetContent> contentCaptor =
                ArgumentCaptor.forClass(BottomSheetContent.class);
        verify(mController).requestShowContent(contentCaptor.capture(), anyBoolean());
        View view = contentCaptor.getValue().getContentView();

        assertNotNull(view);

        TextView titleView = view.findViewById(R.id.mandatory_reauth_opt_in_title);
        TextView explanationView = view.findViewById(R.id.mandatory_reauth_opt_in_explanation);

        // Check that the custom view contains the expected title and explanation.
        assertThat(titleView.getText(), is("Always verify?"));
        assertThat(explanationView.getText(),
                is("For added security on shared devices, turn on verification every time you pay using autofill."));

        Button acceptButton = view.findViewById(R.id.mandatory_reauth_opt_in_accept_button);
        Button cancelButton = view.findViewById(R.id.mandatory_reauth_opt_in_cancel_button);

        // Check that the accept/cancel buttons are correctly shown.
        assertThat(acceptButton.getText(), is("Yes"));
        assertThat(cancelButton.getText(), is("No thanks"));
    }
}
