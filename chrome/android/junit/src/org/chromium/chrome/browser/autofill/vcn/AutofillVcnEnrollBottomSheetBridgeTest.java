// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.LinkedList;

/** Unit test for {@link AutofillVcnEnrollBottomSheetBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetBridgeTest {
    private static final long NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE = 0xa1fabe7a;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private AutofillVcnEnrollBottomSheetBridge.Natives mBridgeNatives;
    @Mock private WebContents mWebContents;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    private ShadowActivity mShadowActivity;
    private WindowAndroid mWindow;
    private AutofillVcnEnrollBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES, false);
        FeatureList.setTestValues(testValues);

        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(AutofillVcnEnrollBottomSheetBridgeJni.TEST_HOOKS, mBridgeNatives);
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mShadowActivity = shadowOf(activity);
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mBridge = new AutofillVcnEnrollBottomSheetBridge();

        when(mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING)).thenReturn(true);
        mBridge.setLayoutStateProviderForTesting(mLayoutStateProvider);

        mBridge.setTabModelSelectorSupplierForTesting(mTabModelSelectorSupplier);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    private void requestShowContent(WebContents webContents) {
        LinkedList<LegalMessageLine> googleLegalMessages = new LinkedList<>();
        googleLegalMessages.add(new LegalMessageLine("Google legal messages."));
        LinkedList<LegalMessageLine> issuerLegalMessages = new LinkedList<>();
        issuerLegalMessages.add(new LegalMessageLine("Issuer legal messages."));

        mBridge.requestShowContent(
                NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE,
                webContents,
                "Message text",
                "Description text. Learn more",
                "Learn more",
                "Card container accessibility description",
                /* issuerIcon= */ Bitmap.createBitmap(
                        /* colors= */ new int[1],
                        /* width= */ 1,
                        /* height= */ 1,
                        Bitmap.Config.ARGB_8888),
                "Card label",
                "Card description",
                googleLegalMessages,
                issuerLegalMessages,
                "Accept button label",
                "Cancel button label",
                "Loading description");
    }

    @Test
    public void testCannotShowWithNullWebContents() {
        requestShowContent(/* webContents= */ null);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testCannotShowWithDestroyedWebContents() {
        when(mWebContents.isDestroyed()).thenReturn(true);

        requestShowContent(mWebContents);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testCannotShowWithoutTopLevelNativeWindow() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(null);

        requestShowContent(mWebContents);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testShowBottomSheet() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);

        requestShowContent(mWebContents);

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillVcnEnrollBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    public void testInitialModelValues() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);

        requestShowContent(mWebContents);

        assertEquals(
                "Message text",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.MESSAGE_TEXT));
        assertEquals(
                "Description text. Learn more",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION)
                        .mText);
        assertEquals(
                "Learn more",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.DESCRIPTION)
                        .mLearnMoreLinkText);
        assertEquals(
                "Card container accessibility description",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(
                                AutofillVcnEnrollBottomSheetProperties
                                        .CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION));
        assertTrue(
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.ISSUER_ICON)
                        .mBitmap
                        .sameAs(
                                Bitmap.createBitmap(
                                        /* colors= */ new int[1],
                                        /* width= */ 1,
                                        /* height= */ 1,
                                        Bitmap.Config.ARGB_8888)));
        assertEquals(
                "Card label",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.CARD_LABEL));
        assertEquals(
                "Card description",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.CARD_DESCRIPTION));
        assertEquals(
                1,
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES)
                        .mLines
                        .size());
        assertEquals(
                "Google legal messages.",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.GOOGLE_LEGAL_MESSAGES)
                        .mLines
                        .get(0)
                        .text);
        assertEquals(
                1,
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES)
                        .mLines
                        .size());
        assertEquals(
                "Issuer legal messages.",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.ISSUER_LEGAL_MESSAGES)
                        .mLines
                        .get(0)
                        .text);
        assertEquals(
                "Accept button label",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.ACCEPT_BUTTON_LABEL));
        assertEquals(
                "Cancel button label",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.CANCEL_BUTTON_LABEL));
        assertFalse(
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE));
        assertEquals(
                "Loading description",
                mBridge.getCoordinatorForTesting()
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.LOADING_DESCRIPTION));
    }

    @Test
    public void testCannotAcceptWithoutShowing() {
        mBridge.onAccept();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testAcceptAfterShowing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);

        mBridge.onAccept();

        verify(mBridgeNatives).onAccept(eq(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE));
    }

    @Test
    public void testSecondAcceptDoesNothing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);
        mBridge.onAccept();
        clearInvocations(mBridgeNatives);

        mBridge.onAccept();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testDismissAfterAcceptDoesNothing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);
        mBridge.onAccept();
        clearInvocations(mBridgeNatives);

        mBridge.onDismiss();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testCannotCancelWithoutShowing() {
        mBridge.onCancel();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testCancelAfterShowing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);

        mBridge.onCancel();

        verify(mBridgeNatives).onCancel(eq(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE));
    }

    @Test
    public void testSecondCancelDoesNothing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);
        mBridge.onCancel();
        clearInvocations(mBridgeNatives);

        mBridge.onCancel();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testDismissAfterCancelDoesNothing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);
        mBridge.onCancel();
        clearInvocations(mBridgeNatives);

        mBridge.onDismiss();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testCannotDismissWithoutShowing() {
        mBridge.onDismiss();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testDismissAfterShowing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);

        mBridge.onDismiss();

        verify(mBridgeNatives).onDismiss(eq(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE));
    }

    @Test
    public void testSecondDismissDoesNothing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);
        mBridge.onDismiss();
        clearInvocations(mBridgeNatives);

        mBridge.onDismiss();

        verifyNoInteractions(mBridgeNatives);
    }

    @Test
    public void testOpenLink() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);

        mBridge.openLink(
                "https://example.test",
                VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK);

        Intent intent = mShadowActivity.getNextStartedActivity();
        assertThat(intent.getData(), equalTo(Uri.parse("https://example.test")));
        assertThat(intent.getAction(), equalTo(Intent.ACTION_VIEW));
        assertThat(
                intent.getExtras().get(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE),
                equalTo(CustomTabsIntent.SHOW_PAGE_TITLE));
        verify(mBridgeNatives)
                .recordLinkClickMetric(
                        eq(NATIVE_AUTOFILL_VCN_ENROLL_BOTTOM_SHEET_BRIDGE),
                        eq(
                                VirtualCardEnrollmentLinkType
                                        .VIRTUAL_CARD_ENROLLMENT_GOOGLE_PAYMENTS_TOS_LINK));
    }

    @Test
    public void testHideWithoutShowing() {
        mBridge.hide();

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testHideAfterShowing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);

        mBridge.hide();

        verify(mBottomSheetController)
                .hideContent(
                        any(),
                        eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testSecondHideDoesNothing() {
        when(mWebContents.isDestroyed()).thenReturn(false);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);
        requestShowContent(mWebContents);
        mBridge.hide();
        clearInvocations(mBottomSheetController);

        mBridge.hide();

        verifyNoInteractions(mBottomSheetController);
    }
}
