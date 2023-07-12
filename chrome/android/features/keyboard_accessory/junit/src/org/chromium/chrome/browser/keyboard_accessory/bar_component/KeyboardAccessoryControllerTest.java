// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.instanceOf;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.AUTOFILL_SUGGESTION;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATION_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import com.google.android.material.tabs.TabLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.PropertyProvider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
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
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
public class KeyboardAccessoryControllerTest {
    @Mock
    private PropertyObserver<PropertyKey> mMockPropertyObserver;
    @Mock
    private ListObservable.ListObserver<Void> mMockActionListObserver;
    @Mock
    private KeyboardAccessoryCoordinator.BarVisibilityDelegate mMockBarVisibilityDelegate;
    @Mock
    private AccessorySheetCoordinator.SheetVisibilityDelegate mMockSheetVisibilityDelegate;
    @Mock
    private KeyboardAccessoryModernView mMockView;
    @Mock
    private KeyboardAccessoryTabLayoutCoordinator mMockTabLayout;
    @Mock
    private KeyboardAccessoryCoordinator.TabSwitchingDelegate mMockTabSwitchingDelegate;
    @Mock
    private AutofillDelegate mMockAutofillDelegate;

    private final KeyboardAccessoryData.Tab mTestTab =
            new KeyboardAccessoryData.Tab("Passwords", null, null, 0, 0, null);

    private KeyboardAccessoryCoordinator mCoordinator;
    private PropertyModel mModel;
    private KeyboardAccessoryMediator mMediator;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);
        setAutofillFeature(false);
        when(mMockView.getTabLayout()).thenReturn(mock(TabLayout.class));
        when(mMockTabLayout.getTabSwitchingDelegate()).thenReturn(mMockTabSwitchingDelegate);
        mCoordinator = new KeyboardAccessoryCoordinator(mMockTabLayout, mMockBarVisibilityDelegate,
                mMockSheetVisibilityDelegate, new FakeViewProvider<>(mMockView));
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
    }

    private void setAutofillFeature(boolean enabled) {
        HashMap<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY, enabled);
        FeatureList.setTestFeatures(features);
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
        PropertyProvider<Action[]> credManProvider =
                new PropertyProvider<>(CREDMAN_CONDITIONAL_UI_REENTRY);

        mCoordinator.registerActionProvider(generationProvider);
        mCoordinator.registerActionProvider(credManProvider);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);

        AutofillSuggestion suggestion1 = new AutofillSuggestion("FirstSuggestion", "",
                /* itemTag= */ "", 0, false, PopupItemId.AUTOCOMPLETE_ENTRY, false, false, false,
                /* featureForIPH= */ "");
        AutofillSuggestion suggestion2 = new AutofillSuggestion("SecondSuggestion", "",
                /* itemTag= */ "", 0, false, PopupItemId.AUTOCOMPLETE_ENTRY, false, false, false,
                /* featureForIPH= */ "");
        Action generationAction = new Action("Generate", GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        Action credManAction =
                new Action("Show Passkeys", CREDMAN_CONDITIONAL_UI_REENTRY, (a) -> {});
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {suggestion1, suggestion2});
        generationProvider.notifyObservers(new Action[] {generationAction});
        credManProvider.notifyObservers(new Action[] {credManAction});

        // Autofill suggestions should always come last before mandatory tab switcher.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4));
        assertThat(mModel.get(BAR_ITEMS).get(0).getAction(), is(credManAction));
        assertThat(mModel.get(BAR_ITEMS).get(1).getAction(), is(generationAction));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem1 = (AutofillBarItem) mModel.get(BAR_ITEMS).get(2);
        assertThat(autofillBarItem1.getSuggestion(), is(suggestion1));
        assertThat(mModel.get(BAR_ITEMS).get(3), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem2 = (AutofillBarItem) mModel.get(BAR_ITEMS).get(3);
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

        AutofillSuggestion suggestion = new AutofillSuggestion("Suggestion", "", /* itemTag= */ "",
                0, false, PopupItemId.AUTOCOMPLETE_ENTRY, false, false, false,
                /* featureForIPH= */ "");
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
    public void testCreatesAddressItemWithIPH() {
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        AutofillSuggestion addressSuggestion =
                new AutofillSuggestion("John", "Main Str", /* itemTag= */ "", 0, false,
                        PopupItemId.ADDRESS_ENTRY, false, false, false, /* featureForIPH= */ "");
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {addressSuggestion, addressSuggestion, addressSuggestion});

        // assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIPH(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE));
        assertThat(getAutofillItemAt(1).getFeatureForIPH(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIPH(), is(nullValue()));
    }

    @Test
    public void testCreatesPaymentItemWithIPH() {
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        AutofillSuggestion paymentSuggestion = new AutofillSuggestion("John", "4828 ****",
                /* itemTag= */ "", 0, false, PopupItemId.CREDIT_CARD_ENTRY, false, false, false,
                /* featureForIPH= */ "");
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {paymentSuggestion, paymentSuggestion, paymentSuggestion});

        // assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIPH(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE));
        assertThat(getAutofillItemAt(1).getFeatureForIPH(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIPH(), is(nullValue()));
    }

    @Test
    public void testIPHFeatureSetForAutofillSuggestion() {
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        AutofillSuggestion paymentSuggestion = new AutofillSuggestion("John", "4828 ****",
                /* itemTag= */ "", 0, false, PopupItemId.CREDIT_CARD_ENTRY, false, false, false,
                FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {paymentSuggestion, paymentSuggestion, paymentSuggestion});

        // assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIPH(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE));
        // Other suggestions also have explicit IPH strings, but only the first suggestion's string
        // is shown.
        assertThat(getAutofillItemAt(1).getFeatureForIPH(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIPH(), is(nullValue()));
    }

    @Test
    public void testCreatesIPHForSecondPasswordItem() {
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        AutofillSuggestion passwordSuggestion1 = new AutofillSuggestion("John", "****",
                /* itemTg= */ "", 0, false, PopupItemId.USERNAME_ENTRY, false, false, false,
                /* featureForIPH= */ "");
        AutofillSuggestion passwordSuggestion2 = new AutofillSuggestion("Eva", "*******",
                /* itemTag= */ "", 0, false, PopupItemId.PASSWORD_ENTRY, false, false, false,
                /* featureForIPH= */ "");
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(new AutofillSuggestion[] {
                passwordSuggestion1, passwordSuggestion2, passwordSuggestion2});

        // assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        assertThat(getAutofillItemAt(1).getFeatureForIPH(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE));
        assertThat(getAutofillItemAt(2).getFeatureForIPH(), is(nullValue()));
    }

    @Test
    public void testCreatesAddressItemWithExternallyProvidedIPH() {
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);
        AutofillSuggestion addressSuggestion = new AutofillSuggestion("John", "Main Str",
                /* itemTag= */ "", 0, false, PopupItemId.ADDRESS_ENTRY, false, false, false,
                FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE);
        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {addressSuggestion, addressSuggestion, addressSuggestion});

        // assertThat(getAutofillItemAt(0).getFeatureForIPH(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIPH(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE));
        assertThat(getAutofillItemAt(1).getFeatureForIPH(), is(nullValue()));
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
    public void testShowSwipingIphUntilVisibilityIsReset() {
        // By default, no IPH is shown but the model holds a callback to notify the mediator.
        mCoordinator.show();
        Callback<Integer> obfuscatedChildAt = mModel.get(OBFUSCATED_CHILD_AT_CALLBACK);
        assertThat(obfuscatedChildAt, notNullValue());
        assertThat(mModel.get(SHOW_SWIPING_IPH), is(false));

        // Notify the mediator to show the IPH because at least one of three items is not visible.
        mModel.get(BAR_ITEMS).add(mock(BarItem.class));
        mModel.get(BAR_ITEMS).add(mock(BarItem.class));
        mModel.get(BAR_ITEMS).add(mock(BarItem.class));
        obfuscatedChildAt.onResult(1);
        assertThat(mModel.get(SHOW_SWIPING_IPH), is(true));

        // Any change that changes the visibility should reset the swiping IPH.
        mModel.set(VISIBLE, false);
        assertThat(mModel.get(SHOW_SWIPING_IPH), is(false));
    }

    @Test
    public void testRecordsAgainIfExistingItemsChange() {
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

        // Adding another action leaves bar impressions unchanged but affects the actions bucket.
        mModel.get(BAR_ITEMS).set(
                new BarItem[] {new BarItem(BarItem.Type.ACTION_BUTTON,
                                       new Action("Uno", GENERATE_PASSWORD_AUTOMATIC, null)),
                        new BarItem(BarItem.Type.ACTION_BUTTON,
                                new Action("Dos", GENERATE_PASSWORD_AUTOMATIC, null))});
        assertThat(getGenerationImpressionCount(), is(2));
    }

    @Test
    public void testModelChangesUpdatesTheContentDescriptionInModernView() {
        setAutofillFeature(true);
        PropertyProvider<AutofillSuggestion[]> autofillSuggestionProvider =
                new PropertyProvider<>(AUTOFILL_SUGGESTION);

        mCoordinator.registerAutofillProvider(autofillSuggestionProvider, mMockAutofillDelegate);
        autofillSuggestionProvider.notifyObservers(
                new AutofillSuggestion[] {mock(AutofillSuggestion.class)});

        assertThat(mModel.get(HAS_SUGGESTIONS), is(true));

        autofillSuggestionProvider.notifyObservers(new AutofillSuggestion[] {});
        assertThat(mModel.get(HAS_SUGGESTIONS), is(false));
    }

    @Test
    public void testFowardsAnimationEventsToVisibilityDelegate() {
        mModel.get(ANIMATION_LISTENER).onFadeInEnd();
        verify(mMockBarVisibilityDelegate).onBarFadeInAnimationEnd();
    }

    private int getGenerationImpressionCount() {
        return RecordHistogram.getHistogramValueCountForTesting(
                ManualFillingMetricsRecorder.UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION,
                AccessoryAction.GENERATE_PASSWORD_AUTOMATIC);
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
