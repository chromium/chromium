// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.ACTIONS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.ACTIVE_TAB;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.TABS;
import static org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryProperties.VISIBLE;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.autofill.keyboard_accessory.KeyboardAccessoryData.PropertyProvider;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.chrome.test.util.browser.modelutil.FakeViewProvider;

/**
 * Controller tests for the keyboard accessory component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
public class KeyboardAccessoryControllerTest {
    @Mock
    private PropertyObserver<PropertyKey> mMockPropertyObserver;
    @Mock
    private ListObservable.ListObserver<Void> mMockTabListObserver;
    @Mock
    private ListObservable.ListObserver<Void> mMockActionListObserver;
    @Mock
    private KeyboardAccessoryCoordinator.VisibilityDelegate mMockVisibilityDelegate;
    @Mock
    private KeyboardAccessoryView mMockView;

    private final KeyboardAccessoryData.Tab mTestTab =
            new KeyboardAccessoryData.Tab(null, null, 0, 0, null);

    private KeyboardAccessoryCoordinator mCoordinator;
    private PropertyModel mModel;
    private KeyboardAccessoryMediator mMediator;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);

        mCoordinator = new KeyboardAccessoryCoordinator(
                mMockVisibilityDelegate, new FakeViewProvider<>(mMockView));
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
    }

    @Test
    public void testCreatesValidSubComponents() {
        assertThat(mCoordinator, is(notNullValue()));
        assertThat(mMediator, is(notNullValue()));
        assertThat(mModel, is(notNullValue()));
    }

    @Test
    public void testModelNotifiesVisibilityChangeOnShowAndHide() {
        mModel.addObserver(mMockPropertyObserver);

        // Setting the visibility on the model should make it propagate that it's visible.
        mModel.set(VISIBLE, true);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, VISIBLE);

        assertThat(mModel.get(VISIBLE), is(true));

        // Resetting the visibility on the model to should make it propagate that it's visible.
        mModel.set(VISIBLE, false);
        verify(mMockPropertyObserver, times(2)).onPropertyChanged(mModel, VISIBLE);
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testChangingTabsNotifiesTabObserver() {
        mModel.get(TABS).addObserver(mMockTabListObserver);

        // Calling addTab on the coordinator should make model propagate that it has a new tab.
        mCoordinator.addTab(mTestTab);
        verify(mMockTabListObserver).onItemRangeInserted(mModel.get(TABS), 0, 1);
        assertThat(mModel.get(TABS).size(), is(1));
        assertThat(mModel.get(TABS).get(0), is(mTestTab));

        // Calling hide on the coordinator should make model propagate that it's invisible.
        mCoordinator.removeTab(mTestTab);
        verify(mMockTabListObserver).onItemRangeRemoved(mModel.get(TABS), 0, 1);
        assertThat(mModel.get(TABS).size(), is(0));
    }

    @Test
    public void testModelNotifiesAboutActionsChangedByProvider() {
        mModel.get(ACTIONS).addObserver(mMockActionListObserver);

        PropertyProvider<Action> testProvider = new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        mCoordinator.registerActionListProvider(testProvider);

        // If the coordinator receives an initial actions, the model should report an insertion.
        mCoordinator.requestShowing();
        Action testAction = new Action(null, 0, null);
        testProvider.notifyObservers(new Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeInserted(mModel.get(ACTIONS), 0, 1);
        assertThat(mModel.get(ACTIONS).size(), is(1));
        assertThat(mModel.get(ACTIONS).get(0), is(equalTo(testAction)));

        // If the coordinator receives a new set of actions, the model should report a change.
        testProvider.notifyObservers(new Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeChanged(mModel.get(ACTIONS), 0, 1, null);
        assertThat(mModel.get(ACTIONS).size(), is(1));
        assertThat(mModel.get(ACTIONS).get(0), is(equalTo(testAction)));

        // If the coordinator receives an empty set of actions, the model should report a deletion.
        testProvider.notifyObservers(new Action[] {});
        verify(mMockActionListObserver).onItemRangeRemoved(mModel.get(ACTIONS), 0, 1);
        assertThat(mModel.get(ACTIONS).size(), is(0));

        // There should be no notification if no actions are reported repeatedly.
        testProvider.notifyObservers(new Action[] {});
        verifyNoMoreInteractions(mMockActionListObserver);
    }

    @Test
    public void testModelDoesntNotifyUnchangedVisibility() {
        mModel.addObserver(mMockPropertyObserver);

        // Setting the visibility on the model should make it propagate that it's visible.
        mModel.set(VISIBLE, true);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, VISIBLE);
        assertThat(mModel.get(VISIBLE), is(true));

        // Marking it as visible again should not result in a notification.
        mModel.set(VISIBLE, true);
        verify(mMockPropertyObserver) // Unchanged number of invocations.
                .onPropertyChanged(mModel, VISIBLE);
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testModelDoesntNotifyUnchangedActiveTab() {
        mModel.addObserver(mMockPropertyObserver);

        assertThat(mModel.get(ACTIVE_TAB), is(nullValue()));
        mModel.set(ACTIVE_TAB, null);
        assertThat(mModel.get(ACTIVE_TAB), is(nullValue()));
        verify(mMockPropertyObserver, never()).onPropertyChanged(mModel, ACTIVE_TAB);

        mModel.set(ACTIVE_TAB, 0);
        assertThat(mModel.get(ACTIVE_TAB), is(0));
        verify(mMockPropertyObserver).onPropertyChanged(mModel, ACTIVE_TAB);

        mModel.set(ACTIVE_TAB, 0);
        assertThat(mModel.get(ACTIVE_TAB), is(0));
        verify(mMockPropertyObserver).onPropertyChanged(mModel, ACTIVE_TAB);
    }

    @Test
    public void testIsVisibleWithSuggestionsBeforeKeyboardComesUp() {
        KeyboardAccessoryData.PropertyProvider<Action> autofillSuggestionProvider =
                new KeyboardAccessoryData.PropertyProvider<>(AUTOFILL_SUGGESTION);
        Action suggestion = new Action("Suggestion", AUTOFILL_SUGGESTION, (a) -> {});
        mCoordinator.registerActionListProvider(autofillSuggestionProvider);

        // Without suggestions, the accessory should remain invisible - even if the keyboard shows.
        assertThat(mModel.get(ACTIONS).size(), is(0));
        assertThat(mModel.get(VISIBLE), is(false));
        mCoordinator.requestShowing();
        assertThat(mModel.get(VISIBLE), is(false));
        mCoordinator.close();

        // Adding suggestions doesn't change the visibility by itself.
        autofillSuggestionProvider.notifyObservers(new Action[] {suggestion, suggestion});
        assertThat(mModel.get(ACTIONS).size(), is(2));
        assertThat(mModel.get(VISIBLE), is(false));

        // But as soon as the keyboard comes up, it should be showing.
        mCoordinator.requestShowing();
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testIsVisibleWithSuggestionsAfterKeyboardComesUp() {
        KeyboardAccessoryData.PropertyProvider<Action> autofillSuggestionProvider =
                new KeyboardAccessoryData.PropertyProvider<>(AUTOFILL_SUGGESTION);
        Action suggestion = new Action("Suggestion", AUTOFILL_SUGGESTION, (a) -> {});
        mCoordinator.registerActionListProvider(autofillSuggestionProvider);

        // Without any suggestions, the accessory should remain invisible.
        assertThat(mModel.get(VISIBLE), is(false));
        assertThat(mModel.get(ACTIONS).size(), is(0));

        // If the keyboard comes up, but there are no suggestions set, keep the accessory hidden.
        mCoordinator.requestShowing();
        assertThat(mModel.get(VISIBLE), is(false));

        // Adding suggestions while the keyboard is visible triggers the accessory.
        autofillSuggestionProvider.notifyObservers(new Action[] {suggestion, suggestion});
        assertThat(mModel.get(ACTIONS).size(), is(2));
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testIsVisibleWithActions() {
        // Without any actions, the accessory should remain invisible.
        assertThat(mModel.get(ACTIONS).size(), is(0));
        mCoordinator.requestShowing();
        assertThat(mModel.get(VISIBLE), is(false));

        // Adding actions while the keyboard is visible triggers the accessory.
        mModel.get(ACTIONS).add(new Action(null, 0, null));
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testSortsActionsBasedOnType() {
        KeyboardAccessoryData.PropertyProvider<Action> generationProvider =
                new KeyboardAccessoryData.PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        KeyboardAccessoryData.PropertyProvider<Action> autofillSuggestionProvider =
                new KeyboardAccessoryData.PropertyProvider<>(AUTOFILL_SUGGESTION);

        mCoordinator.registerActionListProvider(generationProvider);
        mCoordinator.registerActionListProvider(autofillSuggestionProvider);

        Action suggestion1 = new Action("FirstSuggestion", AUTOFILL_SUGGESTION, (a) -> {});
        Action suggestion2 = new Action("SecondSuggestion", AUTOFILL_SUGGESTION, (a) -> {});
        Action generationAction = new Action("Generate", GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        autofillSuggestionProvider.notifyObservers(new Action[] {suggestion1, suggestion2});
        generationProvider.notifyObservers(new Action[] {generationAction});

        // Autofill suggestions should always come last, independent of when they were added.
        assertThat(mModel.get(ACTIONS).size(), is(3));
        assertThat(mModel.get(ACTIONS).indexOf(generationAction), is(0));
        assertThat(mModel.get(ACTIONS).indexOf(suggestion1), is(1));
        assertThat(mModel.get(ACTIONS).indexOf(suggestion2), is(2));
    }

    @Test
    public void testDeletingActionsAffectsOnlyOneType() {
        KeyboardAccessoryData.PropertyProvider<Action> generationProvider =
                new KeyboardAccessoryData.PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        KeyboardAccessoryData.PropertyProvider<Action> autofillSuggestionProvider =
                new KeyboardAccessoryData.PropertyProvider<>(AUTOFILL_SUGGESTION);

        mCoordinator.registerActionListProvider(generationProvider);
        mCoordinator.registerActionListProvider(autofillSuggestionProvider);

        Action suggestion = new Action("NewSuggestion", AUTOFILL_SUGGESTION, (a) -> {});
        Action generationAction = new Action("Generate", GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        autofillSuggestionProvider.notifyObservers(new Action[] {suggestion, suggestion});
        generationProvider.notifyObservers(new Action[] {generationAction});
        assertThat(mModel.get(ACTIONS).size(), is(3));

        // Drop all Autofill suggestions. Only the generation action should remain.
        autofillSuggestionProvider.notifyObservers(new Action[0]);
        assertThat(mModel.get(ACTIONS).size(), is(1));
        assertThat(mModel.get(ACTIONS).indexOf(generationAction), is(0));

        // Readd an Autofill suggestion and drop the generation. Only the suggestion should remain.
        autofillSuggestionProvider.notifyObservers(new Action[] {suggestion});
        generationProvider.notifyObservers(new Action[0]);
        assertThat(mModel.get(ACTIONS).size(), is(1));
        assertThat(mModel.get(ACTIONS).indexOf(suggestion), is(0));
    }

    @Test
    public void testActionsRemovedWhenNotVisible() {
        // Make the accessory visible and add an action to it.
        mCoordinator.requestShowing();
        mModel.get(ACTIONS).add(new Action(null, 0, null));

        // Hiding the accessory should also remove actions.
        mCoordinator.close();
        assertThat(mModel.get(ACTIONS).size(), is(0));
    }

    @Test
    public void testIsVisibleWithTabs() {
        // Without any actions, the accessory should remain invisible.
        assertThat(mModel.get(ACTIONS).size(), is(0));
        mCoordinator.requestShowing();
        assertThat(mModel.get(VISIBLE), is(false));

        // Adding actions while the keyboard is visible triggers the accessory.
        mCoordinator.addTab(mTestTab);
        assertThat(mModel.get(VISIBLE), is(true));
    }

    @Test
    public void testClosingTabDismissesOpenSheet() {
        mModel.set(ACTIVE_TAB, 0);
        mModel.addObserver(mMockPropertyObserver);
        assertThat(mModel.get(ACTIVE_TAB), is(0));

        // Closing the active tab should reset the tab which should trigger the visibility delegate.
        mCoordinator.closeActiveTab();
        assertThat(mModel.get(ACTIVE_TAB), is(nullValue()));
        verify(mMockPropertyObserver).onPropertyChanged(mModel, ACTIVE_TAB);
        verify(mMockVisibilityDelegate).onCloseAccessorySheet();
    }

    @Test
    public void testClosingTabIsNoOpForAlreadyClosedTab() {
        mModel.set(ACTIVE_TAB, null);
        mModel.addObserver(mMockPropertyObserver);

        mCoordinator.closeActiveTab();
        verifyNoMoreInteractions(mMockPropertyObserver, mMockVisibilityDelegate);
    }

    @Test
    public void testRecordsOneImpressionForEveryInitialContentOnVisibilityChange() {
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_BAR_SHOWN),
                is(0));

        // Adding a tab contributes to the tabs and the total bucket.
        mCoordinator.addTab(mTestTab);
        mCoordinator.requestShowing();

        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(1));

        // Adding an action contributes to the actions bucket. Tabs and total are logged again.
        mCoordinator.close(); // Hide, so it's brought up again.
        mModel.get(ACTIONS).add(new Action(null, 0, null));
        mCoordinator.requestShowing();

        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(2));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(2));

        // Adding suggestions adds to the suggestions bucket - and again to tabs and total.
        mCoordinator.close(); // Hide, so it's brought up again.
        KeyboardAccessoryData.PropertyProvider<Action> autofillSuggestionProvider =
                new KeyboardAccessoryData.PropertyProvider<>(AUTOFILL_SUGGESTION);
        Action suggestion = new Action("Suggestion", AUTOFILL_SUGGESTION, (a) -> {});
        mCoordinator.registerActionListProvider(autofillSuggestionProvider);
        autofillSuggestionProvider.notifyObservers(new Action[] {suggestion});
        mCoordinator.requestShowing();

        // Hiding the keyboard clears actions, so don't log more actions from here on out.
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(3));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(3));

        // Removing suggestions adds to everything but the suggestions bucket. The value remains.
        mCoordinator.close(); // Hide, so it's brought up again.
        autofillSuggestionProvider.notifyObservers(new Action[0]);
        mCoordinator.requestShowing();

        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(4));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(4));
    }

    @Test
    public void testRecordsContentBarImpressionOnceAndContentsUpToOnce() {
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_BAR_SHOWN),
                is(0));

        // First showing contains actions only.
        mCoordinator.addTab(mTestTab);
        mCoordinator.requestShowing();

        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(1));

        // Adding a tabs doesn't change the total impression count but the specific bucket.
        mModel.get(ACTIONS).add(new Action(null, 0, null));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(1));

        KeyboardAccessoryData.PropertyProvider<Action> autofillSuggestionProvider =
                new KeyboardAccessoryData.PropertyProvider<>(AUTOFILL_SUGGESTION);
        Action suggestion = new Action("Suggestion", AUTOFILL_SUGGESTION, (a) -> {});
        mCoordinator.registerActionListProvider(autofillSuggestionProvider);
        autofillSuggestionProvider.notifyObservers(new Action[] {suggestion});
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS), is(1));

        // The other changes were not recorded again - just the changes.
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.NO_CONTENTS), is(0));
    }

    @Test
    public void testRecordsAgainIfExistingItemsChange() {
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_BAR_SHOWN),
                is(0));

        // Add a tab and show, so the accessory is permanently visible.
        mCoordinator.addTab(mTestTab);
        mCoordinator.requestShowing();

        // Adding an action fills the bar impression bucket and the actions set once.
        mModel.get(ACTIONS).set(
                new Action[] {new Action("One", AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, null),
                        new Action("Two", AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, null)});
        assertThat(getActionImpressionCount(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));

        // Adding another action leaves bar impressions unchanged but affects the actions bucket.
        mModel.get(ACTIONS).set(
                new Action[] {new Action("Uno", AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, null),
                        new Action("Dos", AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, null)});
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getActionImpressionCount(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC), is(2));
    }

    private int getActionImpressionCount(@AccessoryAction int bucket) {
        return RecordHistogram.getHistogramValueCountForTesting(
                KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION, bucket);
    }

    private int getShownMetricsCount(@AccessoryBarContents int bucket) {
        return RecordHistogram.getHistogramValueCountForTesting(
                KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_BAR_SHOWN, bucket);
    }
}
