// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder.getHistogramForType;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabMetricsRecorder.UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type.CREDIT_CARD_INFO;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type.TITLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.getType;

import android.graphics.drawable.Drawable;
import android.support.v7.widget.RecyclerView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordHistogramJni;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.ui.modelutil.ListObservable;

import java.util.HashMap;

/**
 * Controller tests for the credit card accessory sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
public class CreditCardAccessorySheetControllerTest {
    @Rule
    public JniMocker mocker = new JniMocker();
    @Mock
    private RecyclerView mMockView;
    @Mock
    private ListObservable.ListObserver<Void> mMockItemListObserver;
    @Mock
    private RecordHistogram.Natives mMockRecordHistogramNatives;

    private CreditCardAccessorySheetCoordinator mCoordinator;
    private AccessorySheetTabModel mSheetDataPieces;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        mocker.mock(RecordHistogramJni.TEST_HOOKS, mMockRecordHistogramNatives);
        AccessorySheetTabCoordinator.IconProvider.setIconForTesting(mock(Drawable.class));
        setAutofillFeature(true);
        mCoordinator =
                new CreditCardAccessorySheetCoordinator(RuntimeEnvironment.application, null);
        assertNotNull(mCoordinator);
        mSheetDataPieces = mCoordinator.getSheetDataPiecesForTesting();
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
    public void testModelNotifiesAboutTabDataChangedByProvider() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();

        mSheetDataPieces.addObserver(mMockItemListObserver);
        mCoordinator.registerDataProvider(testProvider);

        // If the coordinator receives a set of initial items, the model should report an insertion.
        testProvider.notifyObservers(
                new AccessorySheetData(AccessoryTabType.CREDIT_CARDS, "Payments", ""));
        verify(mMockItemListObserver).onItemRangeInserted(mSheetDataPieces, 0, 1);
        assertThat(mSheetDataPieces.size(), is(1));

        // If the coordinator receives a new set of items, the model should report a change.
        testProvider.notifyObservers(
                new AccessorySheetData(AccessoryTabType.CREDIT_CARDS, "Other Payments", ""));
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
                new AccessorySheetData(AccessoryTabType.CREDIT_CARDS, "Payments", "");
        testData.getUserInfoList().add(new UserInfo("", null));
        testData.getUserInfoList().get(0).addField(
                new UserInfoField("Todd", "Todd", "", false, field -> {}));
        testData.getUserInfoList().get(0).addField(
                new UserInfoField("**** 9219", "**** 9219", "", true, field -> {}));

        mCoordinator.registerDataProvider(testProvider);
        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(1));
        assertThat(getType(mSheetDataPieces.get(0)), is(CREDIT_CARD_INFO));
        assertThat(mSheetDataPieces.get(0).getDataPiece(), is(testData.getUserInfoList().get(0)));
    }

    @Test
    public void testUsesTitleElementForEmptyState() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        final AccessorySheetData testData =
                new AccessorySheetData(AccessoryTabType.CREDIT_CARDS, "Payments", "");
        mCoordinator.registerDataProvider(testProvider);

        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(1));
        assertThat(getType(mSheetDataPieces.get(0)), is(TITLE));
        assertThat(mSheetDataPieces.get(0).getDataPiece(), is(equalTo("Payments")));

        // As soon UserInfo is available, discard the title.
        testData.getUserInfoList().add(new UserInfo("", null));
        testData.getUserInfoList().get(0).addField(
                new UserInfoField("Todd", "Todd", "", false, field -> {}));
        testData.getUserInfoList().get(0).addField(
                new UserInfoField("**** 9219", "**** 9219", "", true, field -> {}));
        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(1));
        assertThat(getType(mSheetDataPieces.get(0)), is(CREDIT_CARD_INFO));
    }

    @Test
    public void testRecordsNoSuggestionsImpressionsWithoutInteractiveElements() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        mCoordinator.registerDataProvider(testProvider);
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS),
                is(0));
        assertThat(getSuggestionsImpressions(AccessoryTabType.CREDIT_CARDS, 0), is(0));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 0), is(0));

        // If the tab is shown without interactive item, log "0" samples.
        AccessorySheetData accessorySheetData =
                new AccessorySheetData(AccessoryTabType.CREDIT_CARDS, "Payments", "");
        testProvider.notifyObservers(accessorySheetData);
        mCoordinator.onTabShown();

        assertThat(getSuggestionsImpressions(AccessoryTabType.CREDIT_CARDS, 0), is(1));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 0), is(1));
    }

    @Test
    public void testRecordsSelectableSuggestionsImpressionsWhenShown() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        mCoordinator.registerDataProvider(testProvider);
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS),
                is(0));
        assertThat(getSuggestionsImpressions(AccessoryTabType.CREDIT_CARDS, 1), is(0));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 1), is(0));

        // Add only two interactive items - the third one should not be recorded.
        AccessorySheetData accessorySheetData =
                new AccessorySheetData(AccessoryTabType.CREDIT_CARDS, "Payments", "");
        accessorySheetData.getUserInfoList().add(new UserInfo("", null));
        accessorySheetData.getUserInfoList().get(0).addField(
                new UserInfoField("Todd Tester", "Todd Tester", "0", false, result -> {}));
        accessorySheetData.getUserInfoList().get(0).addField(
                new UserInfoField("**** 9219", "Card for Todd Tester", "1", false, result -> {}));
        accessorySheetData.getUserInfoList().get(0).addField(
                new UserInfoField("Unselectable", "Unselectable", "-1", false, null));
        testProvider.notifyObservers(accessorySheetData);
        mCoordinator.onTabShown();

        assertThat(getSuggestionsImpressions(AccessoryTabType.CREDIT_CARDS, 2), is(1));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 2), is(1));
    }

    private int getSuggestionsImpressions(@AccessoryTabType int type, int sample) {
        return RecordHistogram.getHistogramValueCountForTesting(
                getHistogramForType(UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS, type), sample);
    }

    private void setAutofillFeature(boolean enabled) {
        HashMap<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY, enabled);
        ChromeFeatureList.setTestFeatures(features);
    }
}
