// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_TITLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import android.support.design.widget.TabLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordHistogramJni;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryBarContents;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutCoordinator;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.PopupItemId;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.ui.test.util.modelutil.FakeViewProvider;

import java.util.HashMap;

/**
 * Controller tests for the keyboard accessory component.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class, ShadowRecordHistogram.class})
public class KeyboardAccessoryControllerTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private PropertyObserver<PropertyKey> mMockPropertyObserver;
    @Mock
    private ListObservable.ListObserver<Void> mMockActionListObserver;
    @Mock
    private KeyboardAccessoryCoordinator.VisibilityDelegate mMockVisibilityDelegate;
    @Mock
    private KeyboardAccessoryModernView mMockView;
    @Mock
    private KeyboardAccessoryTabLayoutCoordinator mMockTabLayout;
    @Mock
    private KeyboardAccessoryCoordinator.TabSwitchingDelegate mMockTabSwitchingDelegate;
    @Mock
    private AutofillDelegate mMockAutofillDelegate;
    @Mock
    private RecordHistogram.Natives mMockRecordHistogram;

    private final KeyboardAccessoryData.Tab mTestTab =
            new KeyboardAccessoryData.Tab("Passwords", null, null, 0, 0, null);

    private KeyboardAccessoryCoordinator mCoordinator;
    private PropertyModel mModel;
    private KeyboardAccessoryMediator mMediator;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        setAutofillFeature(false);
        mocker.mock(RecordHistogramJni.TEST_HOOKS, mMockRecordHistogram);
        when(mMockView.getTabLayout()).thenReturn(mock(TabLayout.class));
        when(mMockTabLayout.getTabSwitchingDelegate()).thenReturn(mMockTabSwitchingDelegate);
        mCoordinator = new KeyboardAccessoryCoordinator(
                mMockTabLayout, mMockVisibilityDelegate, new FakeViewProvider<>(mMockView));
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
    }

    private void setAutofillFeature(boolean enabled) {
        HashMap<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY, enabled);
        ChromeFeatureList.setTestFeatures(features);
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
    public void testModelNotifiesAboutActionsChangedByProvider() {
        // Set a default tab to prevent visibility changes to trigger now:
        setTabs(new KeyboardAccessoryData.Tab[] {mTestTab});
        mModel.get(BAR_ITEMS).addObserver(mMockActionListObserver);

        PropertyProvider<Action[]> testProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        mCoordinator.registerActionProvider(testProvider);

        // If the coordinator receives an initial actions, the model should report an insertion.
        mCoordinator.show();

        Action testAction = new Action(null, 0, null);
        testProvider.notifyObservers(new Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeInserted(mModel.get(BAR_ITEMS), 0, 1);
        assertThat(mModel.get(BAR_ITEMS).size(), is(1));
        assertThat(mModel.get(BAR_ITEMS).get(0).getAction(), is(equalTo(testAction)));

        // If the coordinator receives a new set of actions, the model should report a change.
        testProvider.notifyObservers(new Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeChanged(mModel.get(BAR_ITEMS), 0, 1, null);
        assertThat(mModel.get(BAR_ITEMS).size(), is(1));
        assertThat(mModel.get(BAR_ITEMS).get(0).getAction(), is(equalTo(testAction)));

        // If the coordinator receives an empty set of actions, the model should report a deletion.
        testProvider.notifyObservers(new Action[] {});
        verify(mMockActionListObserver).onItemRangeRemoved(mModel.get(BAR_ITEMS), 0, 1);
        assertThat(mModel.get(BAR_ITEMS).size(), is(0));

        // There should be no notification if no actions are reported repeatedly.
        testProvider.notifyObservers(new Action[] {});
        verifyNoMoreInteractions(mMockActionListObserver);
    }

    @Test
    public void testModelNotifiesAboutActionsChangedByProviderForRedesign() {
        setAutofillFeature(true);
        // Set a default tab to prevent visibility changes to trigger now:
        setTabs(new KeyboardAccessoryData.Tab[] {mTestTab});
        mModel.get(BAR_ITEMS).addObserver(mMockActionListObserver);

        PropertyProvider<Action[]> testProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        mCoordinator.registerActionProvider(testProvider);

        // If the coordinator receives an initial action, the model should report an insertion.
        mCoordinator.show();

        Action testAction = new Action(null, 0, null);
        testProvider.notifyObservers(new Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeInserted(mModel.get(BAR_ITEMS), 0, 2);
        assertThat(mModel.get(BAR_ITEMS).size(), is(2)); // Plus tab switcher.
        assertThat(mModel.get(BAR_ITEMS).get(0).getAction(), is(equalTo(testAction)));

        // If the coordinator receives a new set of actions, the model should report a change.
        testProvider.notifyObservers(new Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeChanged(mModel.get(BAR_ITEMS), 0, 2, null);
        assertThat(mModel.get(BAR_ITEMS).size(), is(2)); // Plus tab switcher.
        assertThat(mModel.get(BAR_ITEMS).get(0).getAction(), is(equalTo(testAction)));

        // If the coordinator receives an empty set of actions, the model should report a deletion.
        testProvider.notifyObservers(new Action[] {});
        // First call of onItemRangeChanged(mModel.get(BAR_ITEMS), 0, 1, null);
        verify(mMockActionListObserver).onItemRangeRemoved(mModel.get(BAR_ITEMS), 1, 1);
        assertThat(mModel.get(BAR_ITEMS).size(), is(1)); // Only the tab switcher.

        // There should be no notification if no actions are reported repeatedly.
        testProvider.notifyObservers(new Action[] {});
        verify(mMockActionListObserver, times(2))
                .onItemRangeChanged(mModel.get(BAR_ITEMS), 0, 1, null);
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
    public void testTogglesVisibility() {
        mCoordinator.show();
        assertThat(mModel.get(VISIBLE), is(true));
        mCoordinator.dismiss();
        assertThat(mModel.get(VISIBLE), is(false));
    }

    @Test
    public void testSortsActionsBasedOnType() {
        PropertyProvider<Action[]> generationProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);

        mCoordinator.registerActionProvider(generationProvider);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);

        AutofillSuggestion suggestion1 =
                new AutofillSuggestion("FirstSuggestion", "", 0, false, 0, false, false, false);
        AutofillSuggestion suggestion2 =
                new AutofillSuggestion("SecondSuggestion", "", 0, false, 0, false, false, false);
        Action generationAction = new Action("Generate", GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {suggestion1, suggestion2});
        generationProvider.notifyObservers(new Action[] {generationAction});

        // Autofill suggestions should always come last before mandatory tab switcher.
        assertThat(mModel.get(BAR_ITEMS).size(), is(3));
        assertThat(mModel.get(BAR_ITEMS).get(0).getAction(), is(generationAction));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem1 = (AutofillBarItem) mModel.get(BAR_ITEMS).get(1);
        assertThat(autofillBarItem1.getSuggestion(), is(suggestion1));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem2 = (AutofillBarItem) mModel.get(BAR_ITEMS).get(2);
        assertThat(autofillBarItem2.getSuggestion(), is(suggestion2));
    }

    @Test
    public void testMovesTabSwitcherToEndForRedesign() {
        setAutofillFeature(true);
        PropertyProvider<Action[]> generationProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        PropertyProvider<Action[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);

        mCoordinator.registerActionProvider(generationProvider);
        mCoordinator.registerActionProvider(autofillSuggestionProvider);

        Action suggestion1 = new Action("FirstSuggestion", AUTOFILL_SUGGESTION, (a) -> {});
        Action suggestion2 = new Action("SecondSuggestion", AUTOFILL_SUGGESTION, (a) -> {});
        Action generationAction = new Action("Generate", GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        autofillSuggestionProvider.notifyObservers(new Action[] {suggestion1, suggestion2});
        generationProvider.notifyObservers(new Action[] {generationAction});

        // Autofill suggestions should always come last, independent of when they were added.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4)); // Additional tab switcher
        assertThat(mModel.get(BAR_ITEMS).get(0).getAction(), is(generationAction));
        assertThat(mModel.get(BAR_ITEMS).get(1).getAction(), is(suggestion1));
        assertThat(mModel.get(BAR_ITEMS).get(2).getAction(), is(suggestion2));
        assertThat(mModel.get(BAR_ITEMS).get(3).getViewType(), is(BarItem.Type.TAB_LAYOUT));
    }

    @Test
    public void testDeletingActionsAffectsOnlyOneType() {
        PropertyProvider<Action[]> generationProvider =
                new PropertyProvider<>(GENERATE_PASSWORD_AUTOMATIC);
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);

        mCoordinator.registerActionProvider(generationProvider);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);

        AutofillSuggestion suggestion =
                new AutofillSuggestion("Suggestion", "", 0, false, 0, false, false, false);
        Action generationAction = new Action("Generate", GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {suggestion, suggestion});
        generationProvider.notifyObservers(new Action[] {generationAction});
        assertThat(mModel.get(BAR_ITEMS).size(), is(3));

        // Drop all Autofill suggestions. Only the generation action should remain.
        autofillSuggestionProvider.notifyObservers(new AutofillSuggestion[0]);
        assertThat(mModel.get(BAR_ITEMS).size(), is(1));
        assertThat(mModel.get(BAR_ITEMS).get(0).getAction(), is(generationAction));

        // Readd an Autofill suggestion and drop the generation. Only the suggestion should remain.
        autofillSuggestionProvider.notifyObservers(new AutofillSuggestion[] {suggestion});
        generationProvider.notifyObservers(new Action[0]);
        assertThat(mModel.get(BAR_ITEMS).size(), is(1));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem = (AutofillBarItem) mModel.get(BAR_ITEMS).get(0);
        assertThat(autofillBarItem.getSuggestion(), is(suggestion));
    }

    @Test
    public void testGenerationActionsRemovedWhenNotVisible() {
        // Make the accessory visible and add an action to it.
        mCoordinator.show();
        mModel.get(BAR_ITEMS).add(new BarItem(
                BarItem.Type.ACTION_BUTTON, new Action(null, GENERATE_PASSWORD_AUTOMATIC, null)));

        // Hiding the accessory should also remove actions.
        mCoordinator.dismiss();
        assertThat(mModel.get(BAR_ITEMS).size(), is(0));
    }

    @Test
    public void testShowsTitleForActiveTabs() {
        // Add an inactive tab and ensure the sheet title isn't already set.
        mCoordinator.show();
        setTabs(new KeyboardAccessoryData.Tab[] {mTestTab});
        mModel.set(SHEET_TITLE, "");
        assertThat(mCoordinator.hasActiveTab(), is(false));

        // Changing the active tab should also change the title.
        setActiveTab(mTestTab);
        assertThat(mModel.get(SHEET_TITLE), equalTo("Passwords"));
        assertThat(mCoordinator.hasActiveTab(), is(true));
    }

    @Test
    public void testCreatesAddressItemWithIPH() {
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        int suggestionId = 0x1; // The address ID is located in the least 16 bit.
        AutofillSuggestion addressSuggestion = new AutofillSuggestion(
                "John", "Main Str", 0, false, suggestionId, false, false, false);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {addressSuggestion, addressSuggestion, addressSuggestion});

        assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIPH(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE));
        assertThat(getAutofillItemAt(1).getFeatureForIPH(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIPH(), is(nullValue()));
    }

    @Test
    public void testCreatesPaymentItemWithIPH() {
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        int suggestionId = 0x10000; // The payment ID is located in the higher 16 bit.
        AutofillSuggestion paymentSuggestion = new AutofillSuggestion(
                "John", "4828 ****", 0, false, suggestionId, false, false, false);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {paymentSuggestion, paymentSuggestion, paymentSuggestion});

        assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIPH(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE));
        assertThat(getAutofillItemAt(1).getFeatureForIPH(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIPH(), is(nullValue()));
    }

    @Test
    public void testCreatesIPHForSecondPasswordItem() {
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        AutofillSuggestion passwordSuggestion1 = new AutofillSuggestion(
                "John", "****", 0, false, PopupItemId.ITEM_ID_USERNAME_ENTRY, false, false, false);
        AutofillSuggestion passwordSuggestion2 = new AutofillSuggestion("Eva", "*******", 0, false,
                PopupItemId.ITEM_ID_PASSWORD_ENTRY, false, false, false);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(new AutofillSuggestion[] {
                passwordSuggestion1, passwordSuggestion2, passwordSuggestion2});

        assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        assertThat(getAutofillItemAt(1).getFeatureForIPH(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE));
        assertThat(getAutofillItemAt(2).getFeatureForIPH(), is(nullValue()));
    }

    @Test
    public void testSkipAnimationsOnlyUntilNextShow() {
        assertThat(mModel.get(SKIP_CLOSING_ANIMATION), is(false));
        mCoordinator.skipClosingAnimationOnce();
        assertThat(mModel.get(SKIP_CLOSING_ANIMATION), is(true));
        mCoordinator.show();
        assertThat(mModel.get(SKIP_CLOSING_ANIMATION), is(false));
    }

    @Test
    public void testRecordsOneImpressionForEveryInitialContentOnVisibilityChange() {
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_BAR_SHOWN),
                is(0));

        // Adding a tab contributes to the tabs and the total bucket.
        setTabs(new KeyboardAccessoryData.Tab[] {mTestTab});
        mCoordinator.show();

        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(1));

        // Adding an action contributes to the actions bucket. Tabs and total are logged again.
        mCoordinator.dismiss(); // Hide, so it's brought up again.
        mModel.get(BAR_ITEMS).add(new BarItem(
                BarItem.Type.ACTION_BUTTON, new Action(null, GENERATE_PASSWORD_AUTOMATIC, null)));
        mCoordinator.show();

        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(2));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(2));

        // Adding suggestions adds to the suggestions bucket - and again to tabs and total.
        mCoordinator.dismiss(); // Hide, so it's brought up again.
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        AutofillSuggestion suggestion =
                new AutofillSuggestion("Label", "sublabel", 0, false, 0, false, false, false);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(new AutofillSuggestion[] {suggestion});
        mCoordinator.show();

        // Hiding the keyboard clears actions, so don't log more actions from here on out.
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_AUTOFILL_SUGGESTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(3));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(3));

        // Removing suggestions adds to everything but the suggestions bucket. The value remains.
        mCoordinator.dismiss(); // Hide, so it's brought up again.
        autofillSuggestionProvider.notifyObservers(new AutofillSuggestion[0]);
        mCoordinator.show();

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

        // First showing contains tabs only.
        setTabs(new KeyboardAccessoryData.Tab[] {mTestTab});
        mCoordinator.show();

        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_TABS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(1));

        // New actions are recorded in specific buckets but don't affect the general ANY_CONTENTS.
        mModel.get(BAR_ITEMS).add(new BarItem(
                BarItem.Type.ACTION_BUTTON, new Action(null, GENERATE_PASSWORD_AUTOMATIC, null)));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.ANY_CONTENTS), is(1));

        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        AutofillSuggestion suggestion =
                new AutofillSuggestion("Label", "sublabel", 0, false, 0, false, false, false);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(new AutofillSuggestion[] {suggestion});
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
        setTabs(new KeyboardAccessoryData.Tab[] {mTestTab});
        mCoordinator.show();

        // Adding an action fills the bar impression bucket and the actions set once.
        mModel.get(BAR_ITEMS).set(
                new BarItem[] {new BarItem(BarItem.Type.ACTION_BUTTON,
                                       new Action("One", GENERATE_PASSWORD_AUTOMATIC, null)),
                        new BarItem(BarItem.Type.ACTION_BUTTON,
                                new Action("Two", GENERATE_PASSWORD_AUTOMATIC, null))});
        assertThat(getGenerationImpressionCount(), is(1));
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));

        // Adding another action leaves bar impressions unchanged but affects the actions bucket.
        mModel.get(BAR_ITEMS).set(
                new BarItem[] {new BarItem(BarItem.Type.ACTION_BUTTON,
                                       new Action("Uno", GENERATE_PASSWORD_AUTOMATIC, null)),
                        new BarItem(BarItem.Type.ACTION_BUTTON,
                                new Action("Dos", GENERATE_PASSWORD_AUTOMATIC, null))});
        assertThat(getShownMetricsCount(AccessoryBarContents.WITH_ACTIONS), is(1));
        assertThat(getGenerationImpressionCount(), is(2));
    }

    private int getGenerationImpressionCount() {
        return RecordHistogram.getHistogramValueCountForTesting(
                ManualFillingMetricsRecorder.UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION,
                AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
    }

    private int getShownMetricsCount(@AccessoryBarContents int bucket) {
        return RecordHistogram.getHistogramValueCountForTesting(
                KeyboardAccessoryMetricsRecorder.UMA_KEYBOARD_ACCESSORY_BAR_SHOWN, bucket);
    }

    private void setTabs(KeyboardAccessoryData.Tab[] tabs) {
        mCoordinator.setTabs(tabs);
        when(mMockTabSwitchingDelegate.hasTabs()).thenReturn(true);
    }

    private void setActiveTab(KeyboardAccessoryData.Tab tab) {
        when(mMockTabSwitchingDelegate.getActiveTab()).thenReturn(tab);
        when(mMockTabSwitchingDelegate.hasTabs()).thenReturn(true);
        mCoordinator.getMediatorForTesting().onActiveTabChanged(0);
    }

    private AutofillBarItem getAutofillItemAt(int position) {
        return (AutofillBarItem) mModel.get(BAR_ITEMS).get(position);
    }
}
