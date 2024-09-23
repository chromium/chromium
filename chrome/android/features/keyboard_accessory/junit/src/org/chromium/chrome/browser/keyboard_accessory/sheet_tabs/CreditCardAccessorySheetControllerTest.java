// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type.CREDIT_CARD_INFO;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type.PROMO_CODE_INFO;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type.TITLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.getType;

import android.graphics.drawable.Drawable;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PromoCodeInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.modelutil.ListObservable;

/** Controller tests for the credit card accessory sheet. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class})
public class CreditCardAccessorySheetControllerTest {
    @Mock private AccessorySheetTabView mMockView;
    @Mock private ListObservable.ListObserver<Void> mMockItemListObserver;
    @Mock private Profile mMockProfile;
    @Mock private PersonalDataManager mMockPersonalDataManager;

    private CreditCardAccessorySheetCoordinator mCoordinator;
    private AccessorySheetTabItemsModel mSheetDataPieces;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        AccessorySheetTabCoordinator.IconProvider.setIconForTesting(mock(Drawable.class));
        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
        mCoordinator =
                new CreditCardAccessorySheetCoordinator(
                        RuntimeEnvironment.application, mMockProfile, null);
        assertNotNull(mCoordinator);
        mSheetDataPieces = mCoordinator.getSheetDataPiecesForTesting();
    }

    @After
    public void tearDown() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);
    }

    @Test
    public void testCreatesValidTab() {
        KeyboardAccessoryData.Tab tab = mCoordinator.getTab();
        assertNotNull(tab);
        assertNotNull(tab.getIcon());
        assertNotNull(tab.getListener());
    }

    @Test
    public void testSetsViewAdapterOnTabCreation() {
        when(mMockView.getParent()).thenReturn(mMockView);
        KeyboardAccessoryData.Tab tab = mCoordinator.getTab();
        assertNotNull(tab);
        assertNotNull(tab.getListener());
        tab.getListener().onTabCreated(mMockView);
        verify(mMockView).setAdapter(any());
    }

    @Test
    public void testRequestDefaultFocus() {
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(true);

        when(mMockView.getParent()).thenReturn(mMockView);
        KeyboardAccessoryData.Tab tab = mCoordinator.getTab();
        tab.getListener().onTabCreated(mMockView);
        tab.getListener().onTabShown();

        verify(mMockView).requestDefaultA11yFocus();
    }

    @Test
    public void testModelNotifiesAboutTabDataChangedByProvider() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();

        mSheetDataPieces.addObserver(mMockItemListObserver);
        mCoordinator.registerDataProvider(testProvider);

        // If the coordinator receives a set of initial items, the model should report an insertion.
        testProvider.notifyObservers(
                new AccessorySheetData(
                        AccessoryTabType.CREDIT_CARDS,
                        /* userInfoTitle= */ "Payments",
                        /* plusAddressTitle= */ "",
                        /* warning= */ ""));
        verify(mMockItemListObserver).onItemRangeInserted(mSheetDataPieces, 0, 1);
        assertThat(mSheetDataPieces.size(), is(1));

        // If the coordinator receives a new set of items, the model should report a change.
        testProvider.notifyObservers(
                new AccessorySheetData(
                        AccessoryTabType.CREDIT_CARDS,
                        /* userInfoTitle= */ "Other Payments",
                        /* plusAddressTitle= */ "",
                        /* warning= */ ""));
        verify(mMockItemListObserver).onItemRangeChanged(mSheetDataPieces, 0, 1, null);
        assertThat(mSheetDataPieces.size(), is(1));

        // If the coordinator receives an empty set of items, the model should report a deletion.
        testProvider.notifyObservers(null);
        verify(mMockItemListObserver).onItemRangeRemoved(mSheetDataPieces, 0, 1);
        assertThat(mSheetDataPieces.size(), is(0));

        // There should be no notification if no item are reported repeatedly.
        testProvider.notifyObservers(null);
        verifyNoMoreInteractions(mMockItemListObserver);
    }

    @Test
    public void testSplitsTabDataToList() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        final AccessorySheetData testData =
                new AccessorySheetData(
                        AccessoryTabType.CREDIT_CARDS,
                        /* userInfoTitle= */ "",
                        /* plusAddressTitle= */ "",
                        /* warning= */ "");
        testData.getUserInfoList().add(new UserInfo("", false));
        testData.getUserInfoList()
                .get(0)
                .addField(new UserInfoField("Todd", "Todd", "", false, field -> {}));
        testData.getUserInfoList()
                .get(0)
                .addField(new UserInfoField("**** 9219", "**** 9219", "", true, field -> {}));
        testData.getPromoCodeInfoList().add(new PromoCodeInfo());
        testData.getPromoCodeInfoList()
                .get(0)
                .setPromoCode(
                        new UserInfoField(
                                "50$OFF", "Promo Code for Todd Tester", "", false, field -> {}));
        testData.getPromoCodeInfoList()
                .get(0)
                .setDetailsText("Get $50 off when you use this code at checkout.");

        mCoordinator.registerDataProvider(testProvider);
        testProvider.notifyObservers(testData);

        // Tests that promo code offers are ordered before credit cards.
        assertThat(mSheetDataPieces.size(), is(2));
        assertThat(getType(mSheetDataPieces.get(0)), is(PROMO_CODE_INFO));
        assertThat(getType(mSheetDataPieces.get(1)), is(CREDIT_CARD_INFO));
        assertThat(
                mSheetDataPieces.get(0).getDataPiece(), is(testData.getPromoCodeInfoList().get(0)));
        assertThat(mSheetDataPieces.get(1).getDataPiece(), is(testData.getUserInfoList().get(0)));
    }

    @Test
    public void testUsesTitleElementForEmptyState() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        final AccessorySheetData testData =
                new AccessorySheetData(
                        AccessoryTabType.CREDIT_CARDS,
                        /* userInfoTitle= */ "Payments",
                        /* plusAddressTitle= */ "",
                        /* warning= */ "");
        mCoordinator.registerDataProvider(testProvider);

        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(1));
        assertThat(getType(mSheetDataPieces.get(0)), is(TITLE));
        assertThat(mSheetDataPieces.get(0).getDataPiece(), is(equalTo("Payments")));
    }

    @Test
    public void testShowsNoCreditCardsMessageBelowPromoCodes() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        final AccessorySheetData testData =
                new AccessorySheetData(
                        AccessoryTabType.CREDIT_CARDS,
                        /* userInfoTitle= */ "No payment methods",
                        /* plusAddressTitle= */ "",
                        /* warning= */ "");

        testData.getPromoCodeInfoList().add(new PromoCodeInfo());
        testData.getPromoCodeInfoList()
                .get(0)
                .setPromoCode(
                        new UserInfoField(
                                "50$OFF", "Promo Code for Todd Tester", "", false, field -> {}));
        testData.getPromoCodeInfoList()
                .get(0)
                .setDetailsText("Get $50 off when you use this code at checkout.");

        mCoordinator.registerDataProvider(testProvider);
        testProvider.notifyObservers(testData);

        // Tests |mTitle| is shown below promo codes.
        assertThat(mSheetDataPieces.size(), is(2));
        assertThat(getType(mSheetDataPieces.get(0)), is(PROMO_CODE_INFO));
        assertThat(getType(mSheetDataPieces.get(1)), is(TITLE));
        assertThat(mSheetDataPieces.get(1).getDataPiece(), is(equalTo("No payment methods")));
    }
}
