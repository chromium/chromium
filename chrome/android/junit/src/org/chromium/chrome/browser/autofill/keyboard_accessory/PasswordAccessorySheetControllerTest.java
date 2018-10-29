// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.Matchers.equalTo;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.support.v7.widget.RecyclerView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Item;
import org.chromium.chrome.browser.modelutil.ListModel;
import org.chromium.chrome.browser.modelutil.ListObservable;

/**
 * Controller tests for the password accessory sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
public class PasswordAccessorySheetControllerTest {
    @Mock
    private RecyclerView mMockView;
    @Mock
    private ListObservable.ListObserver<Void> mMockItemListObserver;

    private PasswordAccessorySheetCoordinator mCoordinator;
    private ListModel<Item> mModel;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        mCoordinator = new PasswordAccessorySheetCoordinator(RuntimeEnvironment.application);
        assertNotNull(mCoordinator);
        mModel = mCoordinator.getModelForTesting();
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
        KeyboardAccessoryData.Tab tab = mCoordinator.getTab();
        assertNotNull(tab);
        assertNotNull(tab.getListener());
        tab.getListener().onTabCreated(mMockView);
        verify(mMockView).setAdapter(any());
    }

    @Test
    public void testModelNotifiesAboutActionsChangedByProvider() {
        final KeyboardAccessoryData.PropertyProvider<Item> testProvider =
                new KeyboardAccessoryData.PropertyProvider<>();
        final Item testItem = Item.createLabel("Test Item", null);

        mModel.addObserver(mMockItemListObserver);
        mCoordinator.registerItemProvider(testProvider);

        // If the coordinator receives an initial items, the model should report an insertion.
        testProvider.notifyObservers(new Item[] {testItem});
        verify(mMockItemListObserver).onItemRangeInserted(mModel, 0, 1);
        assertThat(mModel.size(), is(1));
        assertThat(mModel.get(0), is(equalTo(testItem)));

        // If the coordinator receives a new set of items, the model should report a change.
        testProvider.notifyObservers(new Item[] {testItem});
        verify(mMockItemListObserver).onItemRangeChanged(mModel, 0, 1, null);
        assertThat(mModel.size(), is(1));
        assertThat(mModel.get(0), is(equalTo(testItem)));

        // If the coordinator receives an empty set of items, the model should report a deletion.
        testProvider.notifyObservers(new Item[] {});
        verify(mMockItemListObserver).onItemRangeRemoved(mModel, 0, 1);
        assertThat(mModel.size(), is(0));

        // There should be no notification if no item are reported repeatedly.
        testProvider.notifyObservers(new Item[] {});
        verifyNoMoreInteractions(mMockItemListObserver);
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
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS),
                is(0));
        assertThat(getSuggestionsImpressions(AccessoryTabType.PASSWORDS, 0), is(0));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 0), is(0));

        // If the tab is shown without interactive item, log "0" samples.
        mModel.set(new Item[] {Item.createLabel("No passwords!", ""), Item.createDivider(),
                Item.createOption("Manage all passwords", "", null),
                Item.createOption("Generate password", "", null)});
        mCoordinator.onTabShown();

        assertThat(getSuggestionsImpressions(AccessoryTabType.PASSWORDS, 0), is(1));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 0), is(1));

        // If the tab is shown with X interactive item, record "X" samples.
        mModel.set(new Item[] {Item.createLabel("Your passwords", ""),
                Item.createSuggestion("Interactive 1", "", false, (v) -> {}, null),
                Item.createSuggestion("Non-interactive 1", "", true, null, null),
                Item.createSuggestion("Interactive 2", "", false, (v) -> {}, null),
                Item.createSuggestion("Non-interactive 2", "", true, null, null),
                Item.createSuggestion("Interactive 3", "", false, (v) -> {}, null),
                Item.createSuggestion("Non-interactive 3", "", true, null, null),
                Item.createDivider(), Item.createOption("Manage all passwords", "", null),
                Item.createOption("Generate password", "", null)});
        mCoordinator.onTabShown();

        assertThat(getSuggestionsImpressions(AccessoryTabType.PASSWORDS, 3), is(1));
        assertThat(getSuggestionsImpressions(AccessoryTabType.ALL, 3), is(1));
    }

    private int getActionImpressions(@AccessoryAction int bucket) {
        return RecordHistogram.getHistogramValueCountForTesting(
                KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION, bucket);
    }

    private int getSuggestionsImpressions(@AccessoryTabType int type, int sample) {
        return RecordHistogram.getHistogramValueCountForTesting(
                KeyboardAccessoryMetricsRecorder.getHistogramForType(
                        KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS,
                        type),
                sample);
    }
}
