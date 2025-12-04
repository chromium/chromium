// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.hamcrest.CoreMatchers.nullValue;
import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY;
import static org.chromium.chrome.browser.keyboard_accessory.AccessoryAction.GENERATE_PASSWORD_AUTOMATIC;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATE_SUGGESTIONS_FROM_TOP;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATION_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS_FIXED;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_STICKY_LAST_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.STYLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ActionBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DismissBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.GroupBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.utils.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.AutofillProfilePayload;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.FillingProduct;
import org.chromium.components.autofill.FillingProductBridgeJni;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.ui.test.util.modelutil.FakeViewProvider;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Controller tests for the keyboard accessory component. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {CustomShadowAsyncTask.class})
@Features.EnableFeatures({
    ChromeFeatureList.AUTOFILL_ANDROID_DESKTOP_KEYBOARD_ACCESSORY_REVAMP,
    ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN,
    ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_WIDTH_ADJUSTMENT,
})
public class KeyboardAccessoryControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PropertyObserver<PropertyKey> mMockPropertyObserver;
    @Mock private ListObservable.ListObserver<Void> mMockActionListObserver;
    @Mock private KeyboardAccessoryCoordinator.BarVisibilityDelegate mMockBarVisibilityDelegate;
    @Mock private AccessorySheetCoordinator.SheetVisibilityDelegate mMockSheetVisibilityDelegate;
    @Mock private KeyboardAccessoryView mMockView;
    @Mock private KeyboardAccessoryButtonGroupCoordinator mMockButtonGroup;
    @Mock private KeyboardAccessoryCoordinator.TabSwitchingDelegate mMockTabSwitchingDelegate;
    @Mock private AutofillDelegate mMockAutofillDelegate;
    @Mock private Profile mMockProfile;
    @Mock private PersonalDataManager mMockPersonalDataManager;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private InsetObserver mInsetObserver;
    @Mock private FillingProductBridgeJni mMockFillingProductBridgeJni;
    @Mock private Supplier<Boolean> mMockIsLargeFormFactorSupplier;
    @Mock private Runnable mMockDismissRunnable;

    private final KeyboardAccessoryData.Tab mTestTab =
            new KeyboardAccessoryData.Tab("Passwords", null, null, 0, 0, null);

    private KeyboardAccessoryCoordinator mCoordinator;
    private PropertyModel mModel;
    private KeyboardAccessoryMediator mMediator;
    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;

    @Before
    public void setUp() {
        when(mMockButtonGroup.getTabSwitchingDelegate()).thenReturn(mMockTabSwitchingDelegate);
        FillingProductBridgeJni.setInstanceForTesting(mMockFillingProductBridgeJni);
        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
        mEdgeToEdgeControllerSupplier = new ObservableSupplierImpl<>(mEdgeToEdgeController);
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(false);

        when(mMockFillingProductBridgeJni.getFillingProductFromSuggestionType(
                        SuggestionType.ADDRESS_ENTRY))
                .thenReturn(FillingProduct.ADDRESS);
        when(mMockFillingProductBridgeJni.getFillingProductFromSuggestionType(
                        SuggestionType.CREDIT_CARD_ENTRY))
                .thenReturn(FillingProduct.CREDIT_CARD);
        when(mMockFillingProductBridgeJni.getFillingProductFromSuggestionType(
                        SuggestionType.IBAN_ENTRY))
                .thenReturn(FillingProduct.IBAN);
        when(mMockFillingProductBridgeJni.getFillingProductFromSuggestionType(
                        SuggestionType.LOYALTY_CARD_ENTRY))
                .thenReturn(FillingProduct.LOYALTY_CARD);

        mCoordinator =
                new KeyboardAccessoryCoordinator(
                        ContextUtils.getApplicationContext(),
                        mMockProfile,
                        mMockButtonGroup,
                        mMockBarVisibilityDelegate,
                        mMockSheetVisibilityDelegate,
                        mEdgeToEdgeControllerSupplier,
                        mInsetObserver,
                        new FakeViewProvider<>(mMockView),
                        mMockIsLargeFormFactorSupplier,
                        mMockDismissRunnable);
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

        assertTrue(mModel.get(VISIBLE));

        // Resetting the visibility on the model to should make it propagate that it's visible.
        mModel.set(VISIBLE, false);
        verify(mMockPropertyObserver, times(2)).onPropertyChanged(mModel, VISIBLE);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testModelNotifiesAboutActionsChangedByProvider() {
        // Set a default tab to prevent visibility changes to trigger now:
        setTabs(new KeyboardAccessoryData.Tab[] {mTestTab});
        mModel.get(BAR_ITEMS).addObserver(mMockActionListObserver);

        Provider<Action[]> testProvider = new Provider<>(GENERATE_PASSWORD_AUTOMATIC);
        mCoordinator.registerActionProvider(testProvider);

        // If the coordinator receives an initial action, the model should report an insertion.
        mCoordinator.show();

        Action testAction = new Action(0, null);
        testProvider.notifyObservers(new Action[] {testAction});
        // 1 item inserted, sheet opener is moved to the end.
        verify(mMockActionListObserver).onItemRangeChanged(mModel.get(BAR_ITEMS), 0, 1, null);
        verify(mMockActionListObserver).onItemRangeInserted(mModel.get(BAR_ITEMS), 1, 1);
        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.size(), is(2)); // Plus tab switcher.
        assertThat(barItems.get(0).getAction(), is(equalTo(testAction)));

        // If the coordinator receives a new set of actions, the model should report a change.
        testProvider.notifyObservers(new Action[] {testAction});
        verify(mMockActionListObserver).onItemRangeChanged(mModel.get(BAR_ITEMS), 0, 2, null);
        barItems = flattenItemGroups();
        assertThat(barItems.size(), is(2)); // Plus tab switcher.
        assertThat(barItems.get(0).getAction(), is(equalTo(testAction)));

        // If the coordinator receives an empty set of actions, the model should report a deletion.
        testProvider.notifyObservers(new Action[] {});
        // First call of onItemRangeChanged(mModel.get(BAR_ITEMS), 0, 1, null);
        verify(mMockActionListObserver).onItemRangeRemoved(mModel.get(BAR_ITEMS), 1, 1);
        assertThat(flattenItemGroups().size(), is(1)); // Only the tab switcher.

        // There should be no notification if no actions are reported repeatedly.
        testProvider.notifyObservers(new Action[] {});
        verify(mMockActionListObserver, times(3))
                .onItemRangeChanged(mModel.get(BAR_ITEMS), 0, 1, null);
        verifyNoMoreInteractions(mMockActionListObserver);
    }

    @Test
    public void testModelDoesntNotifyUnchangedVisibility() {
        mModel.addObserver(mMockPropertyObserver);

        // Setting the visibility on the model should make it propagate that it's visible.
        mModel.set(VISIBLE, true);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, VISIBLE);
        assertTrue(mModel.get(VISIBLE));

        // Marking it as visible again should not result in a notification.
        mModel.set(VISIBLE, true);
        verify(mMockPropertyObserver) // Unchanged number of invocations.
                .onPropertyChanged(mModel, VISIBLE);
        assertTrue(mModel.get(VISIBLE));
    }

    @Test
    public void testTogglesVisibility() {
        mCoordinator.show();
        assertTrue(mModel.get(VISIBLE));
        mCoordinator.dismiss();
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testSortsActionsBasedOnType() {
        Provider<Action[]> generationProvider = new Provider<>(GENERATE_PASSWORD_AUTOMATIC);
        Provider<Action[]> credManProvider = new Provider<>(CREDMAN_CONDITIONAL_UI_REENTRY);

        mCoordinator.registerActionProvider(generationProvider);
        mCoordinator.registerActionProvider(credManProvider);

        AutofillSuggestion suggestion1 =
                new AutofillSuggestion.Builder()
                        .setLabel("FirstSuggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.LOYALTY_CARD_ENTRY)
                        .setFeatureForIph("")
                        .build();
        AutofillSuggestion suggestion2 =
                new AutofillSuggestion.Builder()
                        .setLabel("SecondSuggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .build();
        Action generationAction = new Action(GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        Action credManAction = new Action(CREDMAN_CONDITIONAL_UI_REENTRY, (a) -> {});
        mCoordinator.setSuggestions(List.of(suggestion1, suggestion2), mMockAutofillDelegate);
        generationProvider.notifyObservers(new Action[] {generationAction});
        credManProvider.notifyObservers(new Action[] {credManAction});

        // CredManAction should come later than suggestions but before the tab layout.
        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.size(), is(5));
        assertThat(barItems.get(0).getAction(), is(generationAction));
        assertThat(
                barItems.get(0).getCaptionId(), is(R.string.password_generation_accessory_button));
        assertThat(barItems.get(1), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem1 = (AutofillBarItem) barItems.get(1);
        assertThat(autofillBarItem1.getViewType(), is(BarItem.Type.LOYALTY_CARD_SUGGESTION));
        assertThat(autofillBarItem1.getSuggestion(), is(suggestion1));
        assertThat(barItems.get(2), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem2 = (AutofillBarItem) barItems.get(2);
        assertThat(autofillBarItem2.getViewType(), is(BarItem.Type.SUGGESTION));
        assertThat(autofillBarItem2.getSuggestion(), is(suggestion2));
        assertThat(barItems.get(3).getAction(), is(credManAction));
        assertThat(barItems.get(3).getCaptionId(), is(R.string.select_passkey));
        assertThat(barItems.get(4).getViewType(), is(BarItem.Type.TAB_LAYOUT));
    }

    @Test
    public void testChangesCaptionIdForCredManEntry() {
        Provider<Action[]> credManProvider = new Provider<>(CREDMAN_CONDITIONAL_UI_REENTRY);

        mCoordinator.registerActionProvider(credManProvider);

        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("bulbasaur")
                        .setSubLabel("passkey")
                        .setSuggestionType(SuggestionType.WEBAUTHN_CREDENTIAL)
                        .build();
        Action credManAction = new Action(CREDMAN_CONDITIONAL_UI_REENTRY, (a) -> {});
        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);
        credManProvider.notifyObservers(new Action[] {credManAction});

        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.size(), is(3));
        assertThat(barItems.get(0), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem = (AutofillBarItem) barItems.get(0);
        assertThat(autofillBarItem.getSuggestion(), is(suggestion));
        assertThat(barItems.get(1).getAction(), is(credManAction));
        assertThat(barItems.get(1).getCaptionId(), is(R.string.more_passkeys));
    }

    @Test
    public void testMovesTabSwitcherToEnd() {
        Provider<Action[]> generationProvider = new Provider<>(GENERATE_PASSWORD_AUTOMATIC);

        mCoordinator.registerActionProvider(generationProvider);

        AutofillSuggestion.Builder builder = new AutofillSuggestion.Builder().setSubLabel("");
        AutofillSuggestion suggestion1 = builder.setLabel("kayseri").build();
        AutofillSuggestion suggestion2 = builder.setLabel("spor").build();
        Action generationAction = new Action(GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        mCoordinator.setSuggestions(List.of(suggestion1, suggestion2), mMockAutofillDelegate);
        generationProvider.notifyObservers(new Action[] {generationAction});

        // Autofill suggestions should always come last, independent of when they were added.
        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.size(), is(4)); // Additional tab switcher
        assertThat(barItems.get(0).getAction(), is(generationAction));
        assertThat(barItems.get(1).getViewType(), is(BarItem.Type.SUGGESTION));
        assertThat(((AutofillBarItem) barItems.get(1)).getSuggestion(), is(suggestion1));
        assertThat(barItems.get(2).getViewType(), is(BarItem.Type.SUGGESTION));
        assertThat(((AutofillBarItem) barItems.get(2)).getSuggestion(), is(suggestion2));
        assertThat(barItems.get(3).getViewType(), is(BarItem.Type.TAB_LAYOUT));
    }

    @Test
    public void testDeletingActionsAffectsOnlyOneType() {
        Provider<Action[]> generationProvider = new Provider<>(GENERATE_PASSWORD_AUTOMATIC);

        mCoordinator.registerActionProvider(generationProvider);

        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .build();
        Action generationAction = new Action(GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        mCoordinator.setSuggestions(List.of(suggestion, suggestion), mMockAutofillDelegate);
        generationProvider.notifyObservers(new Action[] {generationAction});
        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.size(), is(4));

        // Drop all Autofill suggestions. Only the generation action should remain.
        mCoordinator.setSuggestions(List.of(), mMockAutofillDelegate);
        barItems = flattenItemGroups();
        assertThat(barItems.size(), is(2));
        assertThat(barItems.get(0).getAction(), is(generationAction));

        // Readd an Autofill suggestion and drop the generation. Only the suggestion should remain.
        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);
        generationProvider.notifyObservers(new Action[0]);
        barItems = flattenItemGroups();
        assertThat(barItems.size(), is(2));
        assertThat(barItems.get(0), instanceOf(AutofillBarItem.class));
        AutofillBarItem autofillBarItem = (AutofillBarItem) barItems.get(0);
        assertThat(autofillBarItem.getSuggestion(), is(suggestion));
    }

    @Test
    public void testGenerationActionsRemovedWhenNotVisible() {
        // Make the accessory visible and add an action to it.
        mCoordinator.show();
        // Ignore tab switcher item.
        assertThat(mModel.get(BAR_ITEMS).size(), is(1));
        mModel.get(BAR_ITEMS)
                .add(
                        new ActionBarItem(
                                BarItem.Type.ACTION_BUTTON,
                                new Action(GENERATE_PASSWORD_AUTOMATIC, null),
                                /* captionId= */ 0));
        assertThat(mModel.get(BAR_ITEMS).size(), is(2));

        // Hiding the accessory should also remove actions.
        mCoordinator.dismiss();
        assertThat(mModel.get(BAR_ITEMS).size(), is(1));
    }

    @Test
    public void testCreatesAddressItemWithIph() {
        AutofillSuggestion addressSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("John")
                        .setSubLabel("Main Str")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(addressSuggestion, addressSuggestion, addressSuggestion),
                mMockAutofillDelegate);

        // assertThat(getAutofillItemAt(0).getFeatureForIph(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(
                getAutofillItemAt(0).getFeatureForIph(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE));
        assertThat(getAutofillItemAt(1).getFeatureForIph(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIph(), is(nullValue()));
    }

    @Test
    public void testCreatesPaymentItemWithIph() {
        AutofillSuggestion paymentSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("John")
                        .setSubLabel("4828 ****")
                        .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(paymentSuggestion, paymentSuggestion, paymentSuggestion),
                mMockAutofillDelegate);

        // assertThat(getAutofillItemAt(0).getFeatureForIph(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(
                getAutofillItemAt(0).getFeatureForIph(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE));
        assertThat(getAutofillItemAt(1).getFeatureForIph(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIph(), is(nullValue()));
    }

    @Test
    public void testIphFeatureSetForAutofillSuggestion() {
        AutofillSuggestion paymentSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("John")
                        .setSubLabel("4828 ****")
                        .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                        .setFeatureForIph(
                                FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE)
                        .build();
        mCoordinator.setSuggestions(
                List.of(paymentSuggestion, paymentSuggestion, paymentSuggestion),
                mMockAutofillDelegate);

        // assertThat(getAutofillItemAt(0).getFeatureForIph(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(
                getAutofillItemAt(0).getFeatureForIph(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE));
        // Other suggestions also have explicit IPH strings, but only the first suggestion's string
        // is shown.
        assertThat(getAutofillItemAt(1).getFeatureForIph(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIph(), is(nullValue()));
    }

    @Test
    public void testCreatesIphForSecondPasswordItem() {
        AutofillSuggestion passwordSuggestion1 =
                new AutofillSuggestion.Builder()
                        .setLabel("John")
                        .setSubLabel("****")
                        .setSuggestionType(SuggestionType.PASSWORD_ENTRY)
                        .setFeatureForIph("")
                        .build();
        AutofillSuggestion passwordSuggestion2 =
                new AutofillSuggestion.Builder()
                        .setLabel("Eva")
                        .setSubLabel("*******")
                        .setSuggestionType(SuggestionType.PASSWORD_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(passwordSuggestion1, passwordSuggestion2, passwordSuggestion2),
                mMockAutofillDelegate);

        // assertThat(getAutofillItemAt(0).getFeatureForIph(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(getAutofillItemAt(0).getFeatureForIph(), is(nullValue()));
        assertThat(
                getAutofillItemAt(1).getFeatureForIph(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE));
        assertThat(getAutofillItemAt(2).getFeatureForIph(), is(nullValue()));
    }

    @Test
    public void testCreatesAddressItemWithExternallyProvidedIph() {
        AutofillSuggestion addressSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("John")
                        .setSubLabel("Man Str")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setFeatureForIph(
                                FeatureConstants
                                        .KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE)
                        .build();

        mCoordinator.setSuggestions(
                List.of(addressSuggestion, addressSuggestion, addressSuggestion),
                mMockAutofillDelegate);

        // assertThat(getAutofillItemAt(0).getFeatureForIph(), is(nullValue()));
        // mCoordinator.prepareUserEducation();
        assertThat(
                getAutofillItemAt(0).getFeatureForIph(),
                is(FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE));
        assertThat(getAutofillItemAt(1).getFeatureForIph(), is(nullValue()));
        assertThat(getAutofillItemAt(2).getFeatureForIph(), is(nullValue()));
    }

    @Test
    public void testSkipAnimationsOnlyUntilNextShow() {
        assertFalse(mModel.get(SKIP_CLOSING_ANIMATION));
        mCoordinator.skipClosingAnimationOnce();
        assertTrue(mModel.get(SKIP_CLOSING_ANIMATION));
        mCoordinator.show();
        assertFalse(mModel.get(SKIP_CLOSING_ANIMATION));
    }

    @Test
    public void testShowSwipingIphUntilVisibilityIsReset() {
        // By default, no IPH is shown but the model holds a callback to notify the mediator.
        mCoordinator.show();
        Callback<Integer> obfuscatedChildAt = mModel.get(OBFUSCATED_CHILD_AT_CALLBACK);
        assertThat(obfuscatedChildAt, notNullValue());
        assertFalse(mModel.get(SHOW_SWIPING_IPH));

        // Notify the mediator to show the IPH because at least one of three items is not visible.
        mModel.get(BAR_ITEMS).add(mock(BarItem.class));
        mModel.get(BAR_ITEMS).add(mock(BarItem.class));
        mModel.get(BAR_ITEMS).add(mock(BarItem.class));
        obfuscatedChildAt.onResult(1);
        assertTrue(mModel.get(SHOW_SWIPING_IPH));

        // Any change that changes the visibility should reset the swiping IPH.
        mModel.set(VISIBLE, false);
        assertFalse(mModel.get(SHOW_SWIPING_IPH));
    }

    @Test
    public void testRecordsAgainIfExistingItemsChange() {
        // Add a tab and show, so the accessory is permanently visible.
        setTabs(new KeyboardAccessoryData.Tab[] {mTestTab});
        mCoordinator.show();

        // Adding an action fills the bar impression bucket and the actions set once.
        mModel.get(BAR_ITEMS)
                .set(
                        new BarItem[] {
                            new ActionBarItem(
                                    BarItem.Type.ACTION_BUTTON,
                                    new Action(GENERATE_PASSWORD_AUTOMATIC, null),
                                    0),
                            new ActionBarItem(
                                    BarItem.Type.ACTION_BUTTON,
                                    new Action(GENERATE_PASSWORD_AUTOMATIC, null),
                                    1)
                        });
        assertThat(getGenerationImpressionCount(), is(1));

        // Adding another action leaves bar impressions unchanged but affects the actions bucket.
        mModel.get(BAR_ITEMS)
                .set(
                        new BarItem[] {
                            new ActionBarItem(
                                    BarItem.Type.ACTION_BUTTON,
                                    new Action(GENERATE_PASSWORD_AUTOMATIC, null),
                                    0),
                            new ActionBarItem(
                                    BarItem.Type.ACTION_BUTTON,
                                    new Action(GENERATE_PASSWORD_AUTOMATIC, null),
                                    1)
                        });
        assertThat(getGenerationImpressionCount(), is(2));
    }

    @Test
    public void testModelChangesUpdatesTheContentDescription() {
        mCoordinator.setSuggestions(List.of(mock(AutofillSuggestion.class)), mMockAutofillDelegate);

        assertTrue(mModel.get(HAS_SUGGESTIONS));

        mCoordinator.setSuggestions(List.of(), mMockAutofillDelegate);
        assertFalse(mModel.get(HAS_SUGGESTIONS));
    }

    @Test
    public void testFowardsAnimationEventsToVisibilityDelegate() {
        mModel.get(ANIMATION_LISTENER).onFadeInEnd();
        verify(mMockBarVisibilityDelegate).onBarFadeInAnimationEnd();
    }

    @Test
    public void testHomeAndWorkBarItems() {
        AutofillProfile profile =
                AutofillProfile.builder().setRecordType(RecordType.ACCOUNT_HOME).build();
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);
        when(mMockPersonalDataManager.getProfile("123")).thenReturn(profile);

        AutofillProfilePayload payload = new AutofillProfilePayload("123");
        AutofillSuggestion addressSuggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("John")
                        .setSubLabel("Main Str")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setPayload(payload)
                        .build();
        mCoordinator.setSuggestions(List.of(addressSuggestion), mMockAutofillDelegate);

        assertThat(getAutofillItemAt(0).getViewType(), is(BarItem.Type.HOME_AND_WORK_SUGGESTION));
    }

    @Test
    public void testStyle() {
        KeyboardAccessoryStyle style =
                new KeyboardAccessoryStyle(
                        /* isDocked= */ true, /* offset= */ 1, /* maxWidth= */ 1);
        mCoordinator.setStyle(style);
        assertThat(mModel.get(STYLE), is(equalTo(style)));
    }

    @Test
    public void testHasStickyLastItem() {
        mCoordinator.setHasStickyLastItem(true);
        assertTrue(mModel.get(HAS_STICKY_LAST_ITEM));

        mCoordinator.setHasStickyLastItem(false);
        assertFalse(mModel.get(HAS_STICKY_LAST_ITEM));
    }

    @Test
    public void testSetAnimateSuggestionsFromTop() {
        mCoordinator.setAnimateSuggestionsFromTop(true);
        assertTrue(mModel.get(ANIMATE_SUGGESTIONS_FROM_TOP));

        mCoordinator.setAnimateSuggestionsFromTop(false);
        assertFalse(mModel.get(ANIMATE_SUGGESTIONS_FROM_TOP));
    }

    @Test
    public void testLargeFormFactorHasDismissButton() {
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(true);

        mCoordinator.setSuggestions(List.of(mock(AutofillSuggestion.class)), mMockAutofillDelegate);

        assertThat(mModel.get(BAR_ITEMS), contains(instanceOf(AutofillBarItem.class)));
        assertThat(
                mModel.get(BAR_ITEMS_FIXED),
                contains(instanceOf(SheetOpenerBarItem.class), instanceOf(DismissBarItem.class)));
    }

    @Test
    public void testLargeFormFactorHasFixedItems() {
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(true);
        Provider<Action[]> generationProvider = new Provider<>(GENERATE_PASSWORD_AUTOMATIC);
        mCoordinator.registerActionProvider(generationProvider);
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .build();
        Action generationAction = new Action(GENERATE_PASSWORD_AUTOMATIC, (a) -> {});

        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);
        generationProvider.notifyObservers(new Action[] {generationAction});

        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.size(), is(2));
        assertThat(barItems.get(0).getAction(), is(generationAction));
        assertThat(barItems.get(1), instanceOf(AutofillBarItem.class));
        assertThat(
                mModel.get(BAR_ITEMS_FIXED),
                contains(instanceOf(SheetOpenerBarItem.class), instanceOf(DismissBarItem.class)));
    }

    @Test
    public void testGroupCreation() {
        Provider<Action[]> generationProvider = new Provider<>(GENERATE_PASSWORD_AUTOMATIC);
        mCoordinator.registerActionProvider(generationProvider);
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(false);

        assertThat(mModel.get(BAR_ITEMS).size(), is(1)); // Only the tab switcher.
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(SheetOpenerBarItem.class));

        final AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("John")
                        .setSubLabel("Main Str")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setFeatureForIph("")
                        .build();

        // Set 1 suggestion and check that no suggestion group is created.
        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);
        assertThat(mModel.get(BAR_ITEMS).size(), is(2));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));

        // Set 2 suggestion and check that a suggestion group is created.
        mCoordinator.setSuggestions(List.of(suggestion, suggestion), mMockAutofillDelegate);
        assertThat(mModel.get(BAR_ITEMS).size(), is(2));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(GroupBarItem.class));
        GroupBarItem suggestionGroup = (GroupBarItem) mModel.get(BAR_ITEMS).get(0);
        assertThat(suggestionGroup.getActionBarItems().size(), is(2));

        // Set 3 suggestions and check that a suggestion group is created again.
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion), mMockAutofillDelegate);
        assertThat(mModel.get(BAR_ITEMS).size(), is(2));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(GroupBarItem.class));
        suggestionGroup = (GroupBarItem) mModel.get(BAR_ITEMS).get(0);
        assertThat(suggestionGroup.getActionBarItems().size(), is(3));

        // Set 4 suggestions and check that a suggestion group is created again, but only for the
        // first 3 suggestions.
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion, suggestion), mMockAutofillDelegate);
        assertThat(mModel.get(BAR_ITEMS).size(), is(3));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(GroupBarItem.class));
        suggestionGroup = (GroupBarItem) mModel.get(BAR_ITEMS).get(0);
        assertThat(suggestionGroup.getActionBarItems().size(), is(3));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));

        // Add the generate password action, which is displayed first in the list of suggestions.
        // Verify that no suggestion group is created, because suggestion group is created only from
        // the suggestions in the beginning of the list.
        final Action generationAction = new Action(GENERATE_PASSWORD_AUTOMATIC, (a) -> {});
        generationProvider.notifyObservers(new Action[] {generationAction});
        assertThat(mModel.get(BAR_ITEMS).size(), is(6));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(ActionBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(3), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(4), instanceOf(AutofillBarItem.class));
    }

    @Test
    public void testGroupCreationForCreditCards() {
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(false);

        final AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Mastercast")
                        .setSubLabel("1234 **")
                        .setSuggestionType(SuggestionType.CREDIT_CARD_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion), mMockAutofillDelegate);

        // It is not allowed to limit width of the credit card suggestions.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
    }

    @Test
    public void testGroupCreationForIbans() {
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(false);

        final AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("DE12 3456 **")
                        .setSubLabel("Your account")
                        .setSuggestionType(SuggestionType.IBAN_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion), mMockAutofillDelegate);

        // It is not allowed to limit width of the IBAN suggestions.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
    }

    @Test
    public void testGroupCreationForPasswords() {
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(false);

        final AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("username")
                        .setSubLabel("******")
                        .setSuggestionType(SuggestionType.PASSWORD_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion), mMockAutofillDelegate);

        // It is not allowed to limit width of the password suggestions.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
    }

    @Test
    public void testGroupCreationWhenStyleIsChanged() {
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(false);
        final AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("John")
                        .setSubLabel("Main Str")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion), mMockAutofillDelegate);

        // Keyboard Accessory is docked initially, make sure that the suggestion groups are not
        // created.
        assertThat(mModel.get(BAR_ITEMS).size(), is(2));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(GroupBarItem.class));

        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(true);
        mCoordinator.setStyle(
                new KeyboardAccessoryStyle(
                        /* isDocked= */ false, /* offset= */ 1, /* maxWidth= */ 1));
        // The suggestions should not be grouped because the style was changed to undocked.
        // TODO: crbug.com/431185714 - Mediator should remove the sheet opener when the style is
        // changed to undocked.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));

        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(false);
        mCoordinator.setStyle(
                new KeyboardAccessoryStyle(
                        /* isDocked= */ true, /* offset= */ 1, /* maxWidth= */ 1));
        // The suggestions should be grouped again since the style was changed to docked.
        assertThat(mModel.get(BAR_ITEMS).size(), is(2));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(GroupBarItem.class));
    }

    @Test
    public void testGroupCreationWhenStyleIsUndocked() {
        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(true);

        final AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("SecondSuggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion), mMockAutofillDelegate);

        // The suggestions should not be grouped because the style was set to undocked.
        assertThat(mModel.get(BAR_ITEMS).size(), is(3));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
    }

    @Test
    public void testGroupCreationWhenNewItemsAreAvailable() {
        Provider<Action[]> credmanActionProvider = new Provider<>(CREDMAN_CONDITIONAL_UI_REENTRY);
        mCoordinator.registerActionProvider(credmanActionProvider);

        when(mMockIsLargeFormFactorSupplier.get()).thenReturn(true);

        final AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("SecondSuggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion), mMockAutofillDelegate);

        // The suggestions should not be grouped because the style was set to undocked.
        assertThat(mModel.get(BAR_ITEMS).size(), is(3));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));

        // The suggestions should not be grouped again after the list of suggestions was updated
        // with a newly available item.
        final Action credmanAction = new Action(CREDMAN_CONDITIONAL_UI_REENTRY, (a) -> {});
        credmanActionProvider.notifyObservers(new Action[] {credmanAction});
        // The suggestions should not be grouped because the style was set to undocked.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
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

    private AutofillBarItem getAutofillItemAt(int position) {
        return (AutofillBarItem) flattenItemGroups().get(position);
    }

    private List<ActionBarItem> flattenItemGroups() {
        List<ActionBarItem> items = new ArrayList<>();
        for (BarItem item : mModel.get(BAR_ITEMS)) {
            items.addAll(item.getActionBarItems());
        }
        return items;
    }
}
