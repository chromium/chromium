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
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.content.Context;
import android.graphics.Typeface;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.MotionEvent;
import android.view.View;
import android.widget.TextView;

import androidx.appcompat.view.ContextThemeWrapper;
import androidx.test.core.app.ApplicationProvider;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiContainer;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiController;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiEntry;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiScope;
import org.chromium.components.browser_ui.adaptive_ui.DismissReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;
import java.util.Set;

/** Tests for {@link NoPasskeysBottomSheetBridge} */
@RunWith(BaseRobolectricTestRunner.class)
public class NoPasskeysBottomSheetModuleTest {
    private static final long TEST_NATIVE = 42069;
    private static final String TEST_ORIGIN = "origin.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private NoPasskeysBottomSheetBridge.Natives mNativeMock;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private AdaptiveUiController mAdaptiveUiController;
    @Mock private MotionEvent mMotionEvent;

    private final Context mContext =
            new ContextThemeWrapper(
                    ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

    private JniRawPtr<NoPasskeysBottomSheetBridge.NativeNoPasskeysBottomSheetBridge> mTestNative;
    private NoPasskeysBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        NoPasskeysBottomSheetBridgeJni.setInstanceForTesting(mNativeMock);
        mTestNative = JniRawPtr.createForTesting(TEST_NATIVE);
        lenient()
                .doReturn(true)
                .when(mBottomSheetController)
                .requestShowContent(any(NoPasskeysBottomSheetContent.class), anyBoolean());
        mBridge =
                new NoPasskeysBottomSheetBridge(
                        mTestNative,
                        new WeakReference<>(mContext),
                        new WeakReference<>(mBottomSheetController),
                        new WeakReference<>(null));
    }

    @After
    public void tearDown() {
        mTestNative.release();
    }

    @Test
    public void callNativeOnDismissAfterShow() {
        mBridge.show(TEST_ORIGIN);
        mBridge.dismiss();
        verify(mNativeMock).onDismissed(mTestNative);
    }

    @Test
    public void dismissOnlyOnce() {
        mBridge.show(TEST_ORIGIN);
        mBridge.dismiss();
        mBridge.dismiss();
        verify(mNativeMock).onDismissed(mTestNative);
    }

    @Test
    public void dismissOnClickingOk() {
        var contentCaptor = ArgumentCaptor.forClass(NoPasskeysBottomSheetContent.class);

        mBridge.show(TEST_ORIGIN);
        verify(mBottomSheetController).requestShowContent(contentCaptor.capture(), eq(true));

        findOkButton(contentCaptor.getValue()).performClick();
        verify(mNativeMock).onDismissed(mTestNative);
    }

    @Test
    public void dismissOnUseAnotherAndInvokeFlow() {
        var contentCaptor = ArgumentCaptor.forClass(NoPasskeysBottomSheetContent.class);

        mBridge.show(TEST_ORIGIN);
        verify(mBottomSheetController).requestShowContent(contentCaptor.capture(), eq(true));

        findUseOtherDeviceButton(contentCaptor.getValue()).performClick();
        verify(mNativeMock).onDismissed(mTestNative);
    }

    @Test
    public void dismissOnHide() {
        var contentCaptor = ArgumentCaptor.forClass(NoPasskeysBottomSheetContent.class);

        mBridge.show(TEST_ORIGIN);
        verify(mBottomSheetController).requestShowContent(contentCaptor.capture(), eq(true));

        // {@code destroy()} is called when a sheet gets dismissed by action, tabs, or layouting.
        contentCaptor.getValue().destroy();
        verify(mNativeMock).onDismissed(mTestNative);
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

        assertEquals(1, spans.length);
        assertEquals(Typeface.BOLD, spans[0].getStyle());
    }

    @Test
    public void adaptiveUi_showBuildsEntryWithExpectedPropertiesAndScope() {
        NoPasskeysBottomSheetBridge bridge = createAdaptiveUiBridge();
        doReturn(AdaptiveUiContainer.BOTTOM_SHEET).when(mAdaptiveUiController).show(any());

        bridge.show(TEST_ORIGIN);

        verifyNoInteractions(mBottomSheetController);
        ArgumentCaptor<AdaptiveUiEntry> entryCaptor =
                ArgumentCaptor.forClass(AdaptiveUiEntry.class);
        verify(mAdaptiveUiController).show(entryCaptor.capture());

        AdaptiveUiEntry entry = entryCaptor.getValue();
        assertEquals(AdaptiveUiScope.TRANSIENT, entry.getScope());
        assertEquals(
                Set.of(AdaptiveUiContainer.BOTTOM_SHEET, AdaptiveUiContainer.MODAL_DIALOG),
                entry.getAllowedContainers());

        PropertyModel model = entry.getPropertyModel();
        assertEquals(
                UserCriticalFeature.TOUCH_TO_FILL_NO_PASSKEYS,
                model.get(AdaptiveUiProperties.SHEET_USER_CRITICAL));
        assertNotNull(model.get(AdaptiveUiProperties.HEADER_ICON));
        assertEquals(
                mContext.getString(R.string.no_passkeys_sheet_title),
                model.get(AdaptiveUiProperties.TITLE));
        CharSequence description = model.get(AdaptiveUiProperties.DESCRIPTION);
        assertNotNull(description);
        int originStart = description.toString().indexOf(TEST_ORIGIN);
        StyleSpan[] spans =
                ((Spanned) description)
                        .getSpans(originStart, originStart + TEST_ORIGIN.length(), StyleSpan.class);
        assertEquals(1, spans.length);
        assertEquals(Typeface.BOLD, spans[0].getStyle());
        assertEquals(
                mContext.getString(R.string.no_passkeys_sheet_ok),
                model.get(AdaptiveUiProperties.PRIMARY_BUTTON_TEXT));
        assertEquals(
                mContext.getString(R.string.no_passkeys_sheet_use_another_device),
                model.get(AdaptiveUiProperties.SECONDARY_BUTTON_TEXT));
        assertNotNull(model.get(AdaptiveUiProperties.DELEGATE));
        assertEquals(
                R.string.no_passkeys_sheet_full_height,
                model.get(AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID));
        assertEquals(
                R.string.no_passkeys_sheet_closed,
                model.get(AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID));
    }

    @Test
    public void adaptiveUi_dismissesWhenQueued() {
        NoPasskeysBottomSheetBridge bridge = createAdaptiveUiBridge();
        doReturn(null).when(mAdaptiveUiController).show(any());
        ArgumentCaptor<AdaptiveUiEntry> entryCaptor =
                ArgumentCaptor.forClass(AdaptiveUiEntry.class);

        bridge.show(TEST_ORIGIN);

        verify(mAdaptiveUiController).show(entryCaptor.capture());
        verify(mNativeMock).onDismissed(mTestNative);
        verify(mAdaptiveUiController)
                .dismiss(entryCaptor.getValue(), DismissReason.DISMISSED_BY_FEATURE);
    }

    @Test
    public void adaptiveUi_observerDismissedNotifiesNativeOnce() {
        NoPasskeysBottomSheetBridge bridge = createAdaptiveUiBridge();
        ArgumentCaptor<AdaptiveUiEntry> entryCaptor =
                ArgumentCaptor.forClass(AdaptiveUiEntry.class);

        bridge.show(TEST_ORIGIN);
        verify(mAdaptiveUiController).show(entryCaptor.capture());

        entryCaptor
                .getValue()
                .getPropertyModel()
                .get(AdaptiveUiProperties.DELEGATE)
                .onDismissed(DismissReason.BACK_PRESS);
        verify(mNativeMock).onDismissed(mTestNative);

        bridge.dismiss();
        verify(mNativeMock).onDismissed(mTestNative);
        verify(mAdaptiveUiController, never())
                .dismiss(any(), eq(DismissReason.DISMISSED_BY_FEATURE));
    }

    @Test
    public void adaptiveUi_secondaryButtonInvokesUseAnotherDeviceAndDismissal() {
        NoPasskeysBottomSheetBridge bridge = createAdaptiveUiBridge();
        ArgumentCaptor<AdaptiveUiEntry> entryCaptor =
                ArgumentCaptor.forClass(AdaptiveUiEntry.class);

        bridge.show(TEST_ORIGIN);
        verify(mAdaptiveUiController).show(entryCaptor.capture());

        AdaptiveUiEntry entry = entryCaptor.getValue();
        entry.getPropertyModel().get(AdaptiveUiProperties.DELEGATE).onSecondaryButtonClicked();
        verify(mNativeMock).onClickUseAnotherDevice(mTestNative);

        entry.getPropertyModel()
                .get(AdaptiveUiProperties.DELEGATE)
                .onDismissed(DismissReason.SECONDARY_ACTION);
        verify(mNativeMock).onDismissed(mTestNative);
    }

    @Test
    public void adaptiveUi_dismissesOnNativeDismissOnlyOnce() {
        NoPasskeysBottomSheetBridge bridge = createAdaptiveUiBridge();
        ArgumentCaptor<AdaptiveUiEntry> entryCaptor =
                ArgumentCaptor.forClass(AdaptiveUiEntry.class);

        bridge.show(TEST_ORIGIN);
        verify(mAdaptiveUiController).show(entryCaptor.capture());

        bridge.dismiss();
        verify(mNativeMock).onDismissed(mTestNative);
        verify(mAdaptiveUiController)
                .dismiss(entryCaptor.getValue(), DismissReason.DISMISSED_BY_FEATURE);

        entryCaptor
                .getValue()
                .getPropertyModel()
                .get(AdaptiveUiProperties.DELEGATE)
                .onDismissed(DismissReason.DISMISSED_BY_FEATURE);
        bridge.dismiss();
        verify(mNativeMock).onDismissed(mTestNative);
    }

    private NoPasskeysBottomSheetBridge createAdaptiveUiBridge() {
        lenient()
                .doReturn(AdaptiveUiContainer.BOTTOM_SHEET)
                .when(mAdaptiveUiController)
                .show(any());
        return new NoPasskeysBottomSheetBridge(
                mTestNative,
                new WeakReference<>(mContext),
                new WeakReference<>(mBottomSheetController),
                new WeakReference<>(mAdaptiveUiController));
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
