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

import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type.ADDRESS_INFO;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabItemsModel.AccessorySheetDataPiece.Type.PLUS_ADDRESS_SECTION;
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

import org.chromium.base.ContextUtils;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.PlusAddressInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.ui.modelutil.ListObservable;

/** Controller tests for the address accessory sheet. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class})
public class AddressAccessorySheetControllerTest {

    @Mock private Profile mProfile;
    @Mock private AccessorySheetTabView mMockView;
    @Mock private ListObservable.ListObserver<Void> mMockItemListObserver;

    private AddressAccessorySheetCoordinator mCoordinator;
    private AccessorySheetTabItemsModel mSheetDataPieces;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockView.getContext()).thenReturn(ContextUtils.getApplicationContext());
        AccessorySheetTabCoordinator.IconProvider.setIconForTesting(mock(Drawable.class));
        mCoordinator =
                new AddressAccessorySheetCoordinator(
                        RuntimeEnvironment.application, mProfile, null);
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
                        AccessoryTabType.ADDRESSES,
                        /* userInfoTitle= */ "Addresses",
                        /* plusAddressTitle= */ "",
                        /* warning= */ ""));
        verify(mMockItemListObserver).onItemRangeInserted(mSheetDataPieces, 0, 1);
        assertThat(mSheetDataPieces.size(), is(1));

        // If the coordinator receives a new set of items, the model should report a change.
        testProvider.notifyObservers(
                new AccessorySheetData(
                        AccessoryTabType.ADDRESSES,
                        /* userInfoTitle= */ "Other Addresses",
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
                        AccessoryTabType.ADDRESSES,
                        /* userInfoTitle= */ "",
                        /* plusAddressTitle= */ "",
                        /* warning= */ "");
        testData.getPlusAddressInfoList()
                .add(
                        new PlusAddressInfo(
                                "google.com",
                                new UserInfoField(
                                        "example@gmail.com",
                                        "example@gmail.com",
                                        "",
                                        false,
                                        field -> {})));
        testData.getUserInfoList().add(new UserInfo("", false));
        testData.getUserInfoList()
                .get(0)
                .addField(new UserInfoField("Name", "Name", "", false, null));
        testData.getUserInfoList()
                .get(0)
                .addField(new UserInfoField("Street", "Street", "", true, field -> {}));

        mCoordinator.registerDataProvider(testProvider);
        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(2));
        assertThat(getType(mSheetDataPieces.get(0)), is(PLUS_ADDRESS_SECTION));
        assertThat(getType(mSheetDataPieces.get(1)), is(ADDRESS_INFO));
        assertThat(
                mSheetDataPieces.get(0).getDataPiece(),
                is(testData.getPlusAddressInfoList().get(0)));
        assertThat(mSheetDataPieces.get(1).getDataPiece(), is(testData.getUserInfoList().get(0)));
    }

    @Test
    public void testUsesTitleElementForEmptyState() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        final AccessorySheetData testData =
                new AccessorySheetData(
                        AccessoryTabType.ADDRESSES,
                        /* userInfoTitle= */ "No addresses",
                        /* plusAddressTitle= */ "No saved plus addresses",
                        /* warning= */ "");
        mCoordinator.registerDataProvider(testProvider);

        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(2));
        assertThat(getType(mSheetDataPieces.get(0)), is(TITLE));
        assertThat(mSheetDataPieces.get(0).getDataPiece(), is(equalTo("No addresses")));
        assertThat(getType(mSheetDataPieces.get(1)), is(TITLE));
        assertThat(mSheetDataPieces.get(1).getDataPiece(), is(equalTo("No saved plus addresses")));
    }
}
