// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.view.ContextThemeWrapper;
import androidx.test.core.app.ApplicationProvider;

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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.lang.ref.WeakReference;

/** Tests for {@link NoPasskeysBottomSheetBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class NoPasskeysBottomSheetModuleTest {
    private static final long TEST_NATIVE = 42069;
    private static final String TEST_ORIGIN = "origin.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private NoPasskeysBottomSheetBridge.Natives mNativeMock;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private MotionEvent mMotionEvent;

    private final Context mContext =
            new ContextThemeWrapper(
                    ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

    private NoPasskeysBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        jniMocker.mock(NoPasskeysBottomSheetBridgeJni.TEST_HOOKS, mNativeMock);
        doReturn(true)
                .when(mBottomSheetController)
                .requestShowContent(any(NoPasskeysBottomSheetContent.class), anyBoolean());
        mBridge =
                new NoPasskeysBottomSheetBridge(
                        TEST_NATIVE,
                        new WeakReference<>(mContext),
                        new WeakReference<>(mBottomSheetController));
    }

    @Test
    public void callNativeOnDismissAfterShow() {
        mBridge.show(TEST_ORIGIN);
        mBridge.dismiss();
        verify(mNativeMock).onDismissed(TEST_NATIVE);
    }

    @Test
    public void dismissOnlyOnce() {
        mBridge.show(TEST_ORIGIN);
        mBridge.dismiss();
        mBridge.dismiss();
        verify(mNativeMock).onDismissed(TEST_NATIVE);
    }

    @Test
    public void dismissOnClickingOk() {
        var contentCaptor = ArgumentCaptor.forClass(NoPasskeysBottomSheetContent.class);

        mBridge.show(TEST_ORIGIN);
        verify(mBottomSheetController).requestShowContent(contentCaptor.capture(), eq(true));

        findOkButton(contentCaptor.getValue()).performClick();
        verify(mNativeMock).onDismissed(TEST_NATIVE);
    }

    @Test
    public void dismissOnUseAnotherAndInvokeFlow() {
        var contentCaptor = ArgumentCaptor.forClass(NoPasskeysBottomSheetContent.class);

        mBridge.show(TEST_ORIGIN);
        verify(mBottomSheetController).requestShowContent(contentCaptor.capture(), eq(true));

        findUseOtherDeviceButton(contentCaptor.getValue()).performClick();
        verify(mNativeMock).onDismissed(TEST_NATIVE);
    }

    @Test
    public void dismissOnHide() {
        var contentCaptor = ArgumentCaptor.forClass(NoPasskeysBottomSheetContent.class);

        mBridge.show(TEST_ORIGIN);
        verify(mBottomSheetController).requestShowContent(contentCaptor.capture(), eq(true));

        // {@code destroy()} is called when a sheet gets dismissed by action, tabs, or layouting.
        contentCaptor.getValue().destroy();
        verify(mNativeMock).onDismissed(TEST_NATIVE);
    }

    @Test
    public void testConsumesGenericMotionEventsToPreventMouseClicksThroughSheet() {
        var contentCaptor = ArgumentCaptor.forClass(NoPasskeysBottomSheetContent.class);

        mBridge.show(TEST_ORIGIN);
        verify(mBottomSheetController).requestShowContent(contentCaptor.capture(), eq(true));

        assertTrue(
                contentCaptor.getValue().getContentView().dispatchGenericMotionEvent(mMotionEvent));
    }

    @Test
    public void originIsBold() {
        var contentCaptor = ArgumentCaptor.forClass(NoPasskeysBottomSheetContent.class);

        mBridge.show(TEST_ORIGIN);
        verify(mBottomSheetController).requestShowContent(contentCaptor.capture(), eq(true));

        TextView view =
                contentCaptor
                        .getValue()
                        .getContentView()
                        .findViewById(R.id.no_passkeys_sheet_subtitle);
        int originStartIndex = view.getText().toString().indexOf(TEST_ORIGIN);
        Spanned spannedMessage = (Spanned) view.getText();
        StyleSpan[] spans =
                spannedMessage.getSpans(
                        originStartIndex, originStartIndex + TEST_ORIGIN.length(), StyleSpan.class);

        assertEquals(spans.length, 1);
        assertEquals(spans[0].getStyle(), Typeface.BOLD);
    }

    private static View findOkButton(NoPasskeysBottomSheetContent content) {
        View okButton = content.getContentView().findViewById(R.id.no_passkeys_ok_button);
        assertNotNull(okButton);
        return okButton;
    }

    private static View findUseOtherDeviceButton(NoPasskeysBottomSheetContent content) {
        View okButton =
                content.getContentView().findViewById(R.id.no_passkeys_use_another_device_button);
        assertNotNull(okButton);
        return okButton;
    }
}
