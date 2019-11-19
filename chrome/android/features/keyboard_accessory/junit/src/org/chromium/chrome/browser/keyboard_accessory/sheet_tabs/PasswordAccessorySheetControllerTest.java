// Copyright 2018 The Chromium Authors. All rights reserved.
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

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder.UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder.getHistogramForType;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabMetricsRecorder.UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type.FOOTER_COMMAND;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type.PASSWORD_INFO;
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
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.AccessorySheetData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.FooterCommand;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.ui.modelutil.ListObservable;

import java.util.HashMap;

/**
 * Controller tests for the password accessory sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
public class PasswordAccessorySheetControllerTest {
    @Rule
    public JniMocker mocker = new JniMocker();
    @Mock
    private RecyclerView mMockView;
    @Mock
    private ListObservable.ListObserver<Void> mMockItemListObserver;
    @Mock
    private RecordHistogram.Natives mMockRecordHistogramNatives;

    private PasswordAccessorySheetCoordinator mCoordinator;
    private AccessorySheetTabModel mSheetDataPieces;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        mocker.mock(RecordHistogramJni.TEST_HOOKS, mMockRecordHistogramNatives);
        AccessorySheetTabCoordinator.IconProvider.setIconForTesting(mock(Drawable.class));
        mCoordinator = new PasswordAccessorySheetCoordinator(RuntimeEnvironment.application, null);
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
                new AccessorySheetData(AccessoryTabType.PASSWORDS, "Passwords", ""));
        verify(mMockItemListObserver).onItemRangeInserted(mSheetDataPieces, 0, 1);
        assertThat(mSheetDataPieces.size(), is(1));

        // If the coordinator receives a new set of items, the model should report a change.
        testProvider.notifyObservers(
                new AccessorySheetData(AccessoryTabType.PASSWORDS, "Other Passwords", ""));
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
        setAutofillFeature(false);
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        final AccessorySheetData testData =
                new AccessorySheetData(AccessoryTabType.PASSWORDS, "Passwords for this site", "");
        testData.getUserInfoList().add(new UserInfo("", null));
        testData.getUserInfoList().get(0).addField(
                new UserInfoField("Name", "Name", "", false, null));
        testData.getUserInfoList().get(0).addField(
                new UserInfoField("Password", "Password for Name", "", true, field -> {}));
        testData.getFooterCommands().add(new FooterCommand("Manage passwords", result -> {}));

        mCoordinator.registerDataProvider(testProvider);
        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(3));
        assertThat(getType(mSheetDataPieces.get(0)), is(TITLE));
        assertThat(getType(mSheetDataPieces.get(1)), is(PASSWORD_INFO));
        assertThat(getType(mSheetDataPieces.get(2)), is(FOOTER_COMMAND));
        assertThat(mSheetDataPieces.get(0).getDataPiece(), is(equalTo("Passwords for this site")));
        assertThat(mSheetDataPieces.get(1).getDataPiece(), is(testData.getUserInfoList().get(0)));
        assertThat(mSheetDataPieces.get(2).getDataPiece(), is(testData.getFooterCommands().get(0)));
    }

    @Test
    public void testUsesTabTitleOnlyForEmptyListsForModernDesign() {
        setAutofillFeature(true);
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        final AccessorySheetData testData =
                new AccessorySheetData(AccessoryTabType.PASSWORDS, "No passwords for this", "");
        mCoordinator.registerDataProvider(testProvider);

        // Providing only FooterCommands and no User Info shows the title as empty state:
        testData.getFooterCommands().add(new FooterCommand("Manage passwords", result -> {}));
        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(2));
        assertThat(getType(mSheetDataPieces.get(0)), is(TITLE));
        assertThat(getType(mSheetDataPieces.get(1)), is(FOOTER_COMMAND));
        assertThat(mSheetDataPieces.get(0).getDataPiece(), is(equalTo("No passwords for this")));

        // As soon UserInfo is available, discard the title.
        testData.getUserInfoList().add(new UserInfo("", null));
        testData.getUserInfoList().get(0).addField(
                new UserInfoField("Name", "Name", "", false, null));
        testData.getUserInfoList().get(0).addField(
                new UserInfoField("Password", "Password for Name", "", true, field -> {}));
        testProvider.notifyObservers(testData);

        assertThat(mSheetDataPieces.size(), is(2));
        assertThat(getType(mSheetDataPieces.get(0)), is(PASSWORD_INFO));
        assertThat(getType(mSheetDataPieces.get(1)), is(FOOTER_COMMAND));
    }

    @Test
    public void testRecordsActionImpressionsWhenShown() {
        assertThat(getActionImpressions(AccessoryAction.MANAGE_PASSWORDS), is(0));

        // Assuming that "Manage Passwords" remains a default option, showing means an impression.
        mCoordinator.onTabShown();

        assertThat(getActionImpressions(AccessoryAction.MANAGE_PASSWORDS), is(1));
    }

    @Test
    public void testRecordsSuggestionsImpressionsWhenShown() {
        final PropertyProvider<AccessorySheetData> testProvider = new PropertyProvider<>();
        mCoordinator.registerDataProvider(testProvider);
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS),
                is(0));
        assertThat(getSuggestionsImpressions(AccessoryTabType.PASSWORDS, 0), is(0));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 0), is(0));

        // If the tab is shown without interactive item, log "0" samples.
        AccessorySheetData accessorySheetData =
                new AccessorySheetData(AccessoryTabType.PASSWORDS, "No passwords!", "");
        accessorySheetData.getFooterCommands().add(new FooterCommand("Manage all passwords", null));
        accessorySheetData.getFooterCommands().add(new FooterCommand("Generate password", null));
        testProvider.notifyObservers(accessorySheetData);
        mCoordinator.onTabShown();

        assertThat(getSuggestionsImpressions(AccessoryTabType.PASSWORDS, 0), is(1));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 0), is(1));

        // If the tab is shown with X interactive item, record "X" samples.
        UserInfo userInfo1 = new UserInfo("", null);
        userInfo1.addField(new UserInfoField("Interactive 1", "", "", false, (v) -> {}));
        userInfo1.addField(new UserInfoField("Non-Interactive 1", "", "", true, null));
        accessorySheetData.getUserInfoList().add(userInfo1);
        UserInfo userInfo2 = new UserInfo("", null);
        userInfo2.addField(new UserInfoField("Interactive 2", "", "", false, (v) -> {}));
        userInfo2.addField(new UserInfoField("Non-Interactive 2", "", "", true, null));
        accessorySheetData.getUserInfoList().add(userInfo2);
        UserInfo userInfo3 = new UserInfo("other.origin.eg", null);
        userInfo3.addField(new UserInfoField("Interactive 3", "", "", false, (v) -> {}));
        userInfo3.addField(new UserInfoField("Non-Interactive 3", "", "", true, null));
        accessorySheetData.getUserInfoList().add(userInfo3);
        testProvider.notifyObservers(accessorySheetData);
        mCoordinator.onTabShown();

        assertThat(getSuggestionsImpressions(AccessoryTabType.PASSWORDS, 3), is(1));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 3), is(1));
    }

    private int getActionImpressions(@AccessoryAction int bucket) {
        return RecordHistogram.getHistogramValueCountForTesting(
                UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION, bucket);
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
