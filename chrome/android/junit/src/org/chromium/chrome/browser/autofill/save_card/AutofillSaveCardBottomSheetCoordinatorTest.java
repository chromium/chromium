// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;

import androidx.annotation.DrawableRes;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.autofill.payments.CardDetail;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link AutofillSaveCardBottomSheetCoordinator} */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveCardBottomSheetCoordinatorTest {
    @DrawableRes private static final int TEST_DRAWABLE_RES = R.drawable.arrow_up;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;
    private ShadowActivity mShadowActivity;
    @Mock private TabModel mTabModel;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private AutofillSaveCardBottomSheetBridge mDelegate;
    private AutofillSaveCardBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mShadowActivity = shadowOf(mActivity);
        mCoordinator =
                new AutofillSaveCardBottomSheetCoordinator(
                        mActivity,
                        uiInfoForTest(),
                        /* skipLoadingForFixFlow= */ false,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mDelegate);
    }

    @Test
    public void testInitialModelValues() {
        // uiInfoForTest() is used during setUp() and the its values should be set in the model.
        assertEquals(
                uiInfoForTest().getTitleText(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.TITLE));
        assertEquals(
                uiInfoForTest().getDescriptionText(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.DESCRIPTION));
        assertEquals(
                uiInfoForTest().getLogoIcon(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.LOGO_ICON));
        assertEquals(
                uiInfoForTest().getCardDescription(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.CARD_DESCRIPTION));
        assertEquals(
                uiInfoForTest().getCardDetail().issuerIconDrawableId,
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.CARD_ICON));
        assertEquals(
                uiInfoForTest().getCardDetail().label,
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.CARD_LABEL));
        assertEquals(
                uiInfoForTest().getCardDetail().subLabel,
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.CARD_SUB_LABEL));
        assertEquals(
                uiInfoForTest().getLegalMessageLines().size(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE)
                        .mLines
                        .size());
        for (int i = 0; i < uiInfoForTest().getLegalMessageLines().size(); i++) {
            assertEquals(
                    uiInfoForTest().getLegalMessageLines().get(i).text,
                    mCoordinator
                            .getPropertyModelForTesting()
                            .get(AutofillSaveCardBottomSheetProperties.LEGAL_MESSAGE)
                            .mLines
                            .get(i)
                            .text);
        }
        assertEquals(
                uiInfoForTest().getConfirmText(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.ACCEPT_BUTTON_LABEL));
        assertEquals(
                uiInfoForTest().getCancelText(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.CANCEL_BUTTON_LABEL));
        assertFalse(
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE));
        assertEquals(
                uiInfoForTest().getLoadingDescription(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveCardBottomSheetProperties.LOADING_DESCRIPTION));
    }

    @Test
    public void testRequestShowContent() {
        mCoordinator.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillSaveCardBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    public void testHide() {
        mCoordinator.requestShowContent();
        mCoordinator.hide(BottomSheetController.StateChangeReason.NONE);

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveCardBottomSheetContent.class),
                        eq(true),
                        eq(BottomSheetController.StateChangeReason.NONE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SAVE_CARD_LOADING_AND_CONFIRMATION})
    public void testClickAccept_withLoadingConfirmation() {
        mCoordinator.requestShowContent();
        mCoordinator.getAutofillSaveCardBottomSheetViewForTesting().mAcceptButton.performClick();

        verify(mDelegate).onUiAccepted();
        verify(mBottomSheetController, times(0)).hideContent(any(), anyBoolean(), anyInt());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SAVE_CARD_LOADING_AND_CONFIRMATION})
    public void testClickAccept_forLocalCard_withLoadingConfirmation() {
        // Create a coordinator for local card. `withIsForUpload` is false for local cards.
        AutofillSaveCardBottomSheetCoordinator coordinator =
                new AutofillSaveCardBottomSheetCoordinator(
                        mActivity,
                        new AutofillSaveCardUiInfo.Builder()
                                .withTitleText("Title text")
                                .withDescriptionText("Description text.")
                                .withIsForUpload(false)
                                .withLogoIcon(TEST_DRAWABLE_RES)
                                .withCardDetail(
                                        new CardDetail(
                                                TEST_DRAWABLE_RES, "Card label", "Card sub label"))
                                .withLegalMessageLines(Collections.EMPTY_LIST)
                                .withConfirmText("Confirm text")
                                .withCancelText("Cancel text")
                                .withLoadingDescription("Loading description")
                                .withCardDescription("")
                                .build(),
                        /* skipLoadingForFixFlow= */ false,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mDelegate);

        coordinator.requestShowContent();
        coordinator.getAutofillSaveCardBottomSheetViewForTesting().mAcceptButton.performClick();

        verify(mDelegate).onUiAccepted();
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveCardBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SAVE_CARD_LOADING_AND_CONFIRMATION})
    public void testClickAccept_whenLoadingDisabled_withLoadingConfirmation() {
        // Create a coordinator with `skipLoadingForFixFlow` set as true.
        AutofillSaveCardBottomSheetCoordinator coordinator =
                new AutofillSaveCardBottomSheetCoordinator(
                        mActivity,
                        uiInfoForTest(),
                        /* skipLoadingForFixFlow= */ true,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        mDelegate);

        coordinator.requestShowContent();
        coordinator.getAutofillSaveCardBottomSheetViewForTesting().mAcceptButton.performClick();

        verify(mDelegate).onUiAccepted();
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveCardBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SAVE_CARD_LOADING_AND_CONFIRMATION})
    public void testClickAccept_withoutLoadingConfirmation() {
        mCoordinator.requestShowContent();
        mCoordinator.getAutofillSaveCardBottomSheetViewForTesting().mAcceptButton.performClick();

        verify(mDelegate).onUiAccepted();
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveCardBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testClickCancel() {
        mCoordinator.requestShowContent();
        mCoordinator.getAutofillSaveCardBottomSheetViewForTesting().mCancelButton.performClick();

        verify(mDelegate).onUiCanceled();
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveCardBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testOpenLegalMessageLink() {
        final String urlString = "https://example.test";

        mCoordinator.requestShowContent();
        mCoordinator.openLegalMessageLink(urlString);

        Intent intent = mShadowActivity.getNextStartedActivity();
        assertEquals(Uri.parse(urlString), intent.getData());
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(
                CustomTabsIntent.SHOW_PAGE_TITLE,
                intent.getExtras().get(CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE));
    }

    private AutofillSaveCardUiInfo uiInfoForTest() {
        return new AutofillSaveCardUiInfo.Builder()
                .withTitleText("Title text")
                .withDescriptionText("Description text.")
                .withIsForUpload(true)
                .withLogoIcon(TEST_DRAWABLE_RES)
                .withCardDetail(new CardDetail(TEST_DRAWABLE_RES, "Card label", "Card sub label"))
                .withLegalMessageLines(List.of(new LegalMessageLine("Legal message line")))
                .withConfirmText("Confirm text")
                .withCancelText("Cancel text")
                .withLoadingDescription("Loading description")
                .withCardDescription("")
                .build();
    }
}
