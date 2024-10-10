// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSpecs;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Optional;

/** Unit test for {@link AutofillVcnEnrollBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
@EnableFeatures({
    ChromeFeatureList.AUTOFILL_ENABLE_NEW_CARD_ART_AND_NETWORK_IMAGES,
    AutofillFeatures.AUTOFILL_ENABLE_VIRTUAL_CARD_JAVA_PAYMENTS_DATA_MANAGER
})
public final class AutofillVcnEnrollBottomSheetCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private Profile mProfile;
    @Mock private PersonalDataManager mPersonalDataManager;

    private WindowAndroid mWindow;
    private AutofillVcnEnrollBottomSheetCoordinator mCoordinator;
    private boolean mAcceptClicked;
    private boolean mCancelClicked;

    @Before
    public void setUp() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING)).thenReturn(true);

        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
        Activity activity = buildActivity(Activity.class).create().get();
        mWindow = new WindowAndroid(activity);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        setUpCreditCardWithCardArtUrl();
        mCoordinator =
                new AutofillVcnEnrollBottomSheetCoordinator(
                        mWindow.getContext().get(),
                        mProfile,
                        new PropertyModel.Builder(AutofillVcnEnrollBottomSheetProperties.ALL_KEYS),
                        mLayoutStateProvider,
                        mTabModelSelectorSupplier,
                        new AutofillVcnEnrollBottomSheetCoordinator.Delegate() {
                            @Override
                            public void onAccept() {
                                mAcceptClicked = true;
                            }

                            @Override
                            public void onCancel() {
                                mCancelClicked = true;
                            }

                            @Override
                            public void onDismiss() {}
                        });
    }

    private void setUpCreditCardWithCardArtUrl() {
        String cardArtUrl = "http://example.test/card.png";
        Bitmap bitmap = Bitmap.createBitmap(/* width= */ 5, /* height= */ 5, Config.ARGB_8888);
        when(mPersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable(
                        new GURL(cardArtUrl),
                        CardIconSpecs.create(mWindow.getContext().get(), ImageSize.SMALL)))
                .thenReturn(Optional.of(bitmap));
        when(mPersonalDataManager.getCustomImageForAutofillSuggestionIfAvailable(
                        /* customImageUrl= */ any(), /* cardIconSpecs= */ any()))
                .thenReturn(Optional.empty());
    }

    @After
    public void tearDown() {
        mAcceptClicked = false;
        mCancelClicked = false;
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    public void testShow() {
        mCoordinator.requestShowContent(mWindow);

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillVcnEnrollBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    public void testCannotShowWhenNotInBrowsingLayoutType() {
        // E.g., when in tab overview layout type.
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.BROWSING)).thenReturn(false);

        mCoordinator.requestShowContent(mWindow);

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    public void testHideAfterShow() {
        mCoordinator.requestShowContent(mWindow);
        mCoordinator.hide();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillVcnEnrollBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testHideWithoutShow() {
        mCoordinator.hide();

        verifyNoInteractions(mBottomSheetController);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION})
    public void testClickAccept_enablesLoadingStateAndDoesNotDismissTheBottomSheet() {
        mCoordinator.requestShowContent(mWindow);
        mCoordinator.getAutofillVcnEnrollBottomSheetViewForTesting().mAcceptButton.performClick();

        assertTrue(mAcceptClicked);
        assertTrue(
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE));
        verify(mBottomSheetController, times(0)).hideContent(any(), anyBoolean(), anyInt());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_VCN_ENROLL_LOADING_AND_CONFIRMATION})
    public void testClickAccept_dismissesTheBottomSheet() {
        mCoordinator.requestShowContent(mWindow);
        mCoordinator.getAutofillVcnEnrollBottomSheetViewForTesting().mAcceptButton.performClick();

        assertTrue(mAcceptClicked);
        assertFalse(
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillVcnEnrollBottomSheetProperties.SHOW_LOADING_STATE));
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillVcnEnrollBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testClickCancelDismissesTheBottomSheet() {
        mCoordinator.requestShowContent(mWindow);

        mCoordinator.getAutofillVcnEnrollBottomSheetViewForTesting().mCancelButton.performClick();

        assertTrue(mCancelClicked);
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillVcnEnrollBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }
}
