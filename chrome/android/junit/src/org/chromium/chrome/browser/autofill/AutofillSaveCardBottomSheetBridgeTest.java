// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link AutofillSaveCardBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveCardBottomSheetBridgeTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AutofillSaveCardBottomSheetBridge mAutofillSaveCardBottomSheetBridge;

    private WindowAndroid mWindow;

    @Mock
    private ManagedBottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        Context mApplicationContext = ApplicationProvider.getApplicationContext();
        mWindow = new WindowAndroid(mApplicationContext);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mAutofillSaveCardBottomSheetBridge = new AutofillSaveCardBottomSheetBridge(mWindow);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    @SmallTest
    public void requestShowContent_callsControllerRequestShowContent() {
        mAutofillSaveCardBottomSheetBridge.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillSaveCardBottomSheetBridge.BottomSheetContentImpl.class),
                        /* animate= */ eq(true));
    }

    @Test
    @SmallTest
    public void requestShowContent_bottomSheetContentImplIsStubbed() {
        mAutofillSaveCardBottomSheetBridge.requestShowContent();

        ArgumentCaptor<AutofillSaveCardBottomSheetBridge.BottomSheetContentImpl> contentCaptor =
                ArgumentCaptor.forClass(
                        AutofillSaveCardBottomSheetBridge.BottomSheetContentImpl.class);
        verify(mBottomSheetController)
                .requestShowContent(contentCaptor.capture(), /* animate= */ anyBoolean());
        AutofillSaveCardBottomSheetBridge.BottomSheetContentImpl content = contentCaptor.getValue();
        assertThat(content.getContentView(), notNullValue());
        assertThat(content.getSheetContentDescriptionStringId(), equalTo(android.R.string.ok));
        assertThat(content.getSheetHalfHeightAccessibilityStringId(), equalTo(android.R.string.ok));
        assertThat(content.getSheetFullHeightAccessibilityStringId(), equalTo(android.R.string.ok));
        assertThat(content.getSheetClosedAccessibilityStringId(), equalTo(android.R.string.ok));
    }
}
