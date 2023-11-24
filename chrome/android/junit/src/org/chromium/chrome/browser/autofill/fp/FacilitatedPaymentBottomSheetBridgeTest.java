// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.fp;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link FacilitatedPaymentBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FacilitatedPaymentBottomSheetBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private ManagedBottomSheetController mBottomSheetController;

    private FacilitatedPaymentBottomSheetBridge mFacilitatedPaymentBottomSheetBridge;
    private WindowAndroid mWindow;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context mApplicationContext = ApplicationProvider.getApplicationContext();
        mWindow = new WindowAndroid(mApplicationContext);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mFacilitatedPaymentBottomSheetBridge = new FacilitatedPaymentBottomSheetBridge();
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    @SmallTest
    public void requestShowContent_callsControllerRequestShowContent() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);

        mFacilitatedPaymentBottomSheetBridge.requestShowContent(mWebContents);

        verify(mBottomSheetController)
                .requestShowContent(
                        any(FacilitatedPaymentBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    @SmallTest
    public void requestShowContent_bottomSheetContentImplIsStubbed() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);

        mFacilitatedPaymentBottomSheetBridge.requestShowContent(mWebContents);

        ArgumentCaptor<FacilitatedPaymentBottomSheetContent> contentCaptor =
                ArgumentCaptor.forClass(FacilitatedPaymentBottomSheetContent.class);
        verify(mBottomSheetController)
                .requestShowContent(contentCaptor.capture(), /* animate= */ anyBoolean());
        FacilitatedPaymentBottomSheetContent content = contentCaptor.getValue();
        assertThat(content.getContentView(), notNullValue());
        assertThat(content.getSheetContentDescriptionStringId(), equalTo(R.string.ok));
        assertThat(content.getSheetHalfHeightAccessibilityStringId(), equalTo(R.string.ok));
        assertThat(content.getSheetFullHeightAccessibilityStringId(), equalTo(R.string.ok));
        assertThat(content.getSheetClosedAccessibilityStringId(), equalTo(R.string.ok));
    }
}
