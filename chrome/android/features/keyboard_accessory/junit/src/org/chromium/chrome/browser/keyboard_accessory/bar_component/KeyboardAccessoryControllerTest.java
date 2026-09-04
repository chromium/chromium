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
import static org.hamcrest.Matchers.hasItem;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
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
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISMISS_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_STICKY_LAST_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SELECTED_SUGGESTION_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.STYLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import android.widget.Button;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
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
import org.chromium.components.autofill.AutofillAiPayload;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.autofill.AutofillProfilePayload;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.FillingProduct;
import org.chromium.components.autofill.FillingProductBridgeJni;
import org.chromium.components.autofill.RecordType;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.ui.test.util.modelutil.FakeViewProvider;

import java.util.ArrayList;
import java.util.List;

/** Controller tests for the keyboard accessory component. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({
    ChromeFeatureList.AUTOFILL_AI_LIMIT_SUGGESTION_WIDTH,
    ChromeFeatureList.AUTOFILL_ANDROID_DESKTOP_KEYBOARD_ACCESSORY_REVAMP,
    ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING,
    ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_HOVER_PREVIEW,
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
    @Mock private EntityDataManager mMockEntityDataManager;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private KeyboardAccessoryCoordinator.AtMemoryDelegate mMockAtMemoryDelegate;
    @Mock private InsetObserver mInsetObserver;
    @Mock private FillingProductBridgeJni mMockFillingProductBridgeJni;
    @Mock private Runnable mMockDismissRunnable;
    @Mock private Runnable mMockAtMemoryCallback;
    @Mock private ModalDialogManager mModalDialogManager;

    private final KeyboardAccessoryData.Tab mTestTab =
            new KeyboardAccessoryData.Tab("Passwords", 0, null, 0, 0, null);

    private KeyboardAccessoryCoordinator mCoordinator;
    private PropertyModel mModel;
    private KeyboardAccessoryMediator mMediator;
    private SettableNonNullObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;

    @Before
    public void setUp() {
        ApplicationProvider.getApplicationContext().setTheme(R.style.Theme_BrowserUI_DayNight);
        when(mMockButtonGroup.getTabSwitchingDelegate()).thenReturn(mMockTabSwitchingDelegate);
        FillingProductBridgeJni.setInstanceForTesting(mMockFillingProductBridgeJni);
        PersonalDataManagerFactory.setInstanceForTesting(mMockPersonalDataManager);
        EntityDataManagerFactory.setInstanceForTesting(mMockEntityDataManager);
        mEdgeToEdgeControllerSupplier = ObservableSuppliers.createNonNull(mEdgeToEdgeController);

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
        when(mMockFillingProductBridgeJni.getFillingProductFromSuggestionType(
                        SuggestionType.FILL_AUTOFILL_AI))
                .thenReturn(FillingProduct.AUTOFILL_AI);

        when(mMockButtonGroup.getAtMemoryDelegate()).thenReturn(mMockAtMemoryDelegate);
        mCoordinator =
                new KeyboardAccessoryCoordinator(
                        ApplicationProvider.getApplicationContext(),
                        mMockProfile,
                        mModalDialogManager,
                        mMockButtonGroup,
                        mMockBarVisibilityDelegate,
                        mMockSheetVisibilityDelegate,
                        mEdgeToEdgeControllerSupplier,
                        mInsetObserver,
                        new FakeViewProvider<>(mMockView),
                        mMockDismissRunnable);
        mMediator = mCoordinator.getMediatorForTesting();
        mModel = mMediator.getModelForTesting();
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
    }

    @Test
    public void testSetsAtMemoryCallback() {
        mCoordinator.setAtMemoryCallback(mMockAtMemoryCallback);
        verify(mMockAtMemoryDelegate).setAtMemoryCallback(mMockAtMemoryCallback);
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
        Action generationAction =
                new Action(GENERATE_PASSWORD_AUTOMATIC, CallbackUtils.emptyCallback());
        Action credManAction =
                new Action(CREDMAN_CONDITIONAL_UI_REENTRY, CallbackUtils.emptyCallback());
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
        Action credManAction =
                new Action(CREDMAN_CONDITIONAL_UI_REENTRY, CallbackUtils.emptyCallback());
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
        Action generationAction =
                new Action(GENERATE_PASSWORD_AUTOMATIC, CallbackUtils.emptyCallback());
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
        Action generationAction =
                new Action(GENERATE_PASSWORD_AUTOMATIC, CallbackUtils.emptyCallback());
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
    public void testSuggestionAcceptanceUpdatesSuggestions() {
        AutofillSuggestion suggestion1 =
                new AutofillSuggestion.Builder()
                        .setLabel("Loading Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .setPayload(new AutofillAiPayload("guid", /* requiresServerFetch= */ true))
                        .build();

        AutofillSuggestion suggestion2 =
                new AutofillSuggestion.Builder()
                        .setLabel("Other Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .setPayload(new AutofillAiPayload("guid", /* requiresServerFetch= */ false))
                        .build();

        mCoordinator.setSuggestions(List.of(suggestion1, suggestion2), mMockAutofillDelegate);

        assertTrue(getAutofillItemAt(0).isEnabled());
        assertTrue(getAutofillItemAt(1).isEnabled());

        // Simulate a click on the first suggestion.
        getAutofillItemAt(0).getAction().getCallback().onResult(getAutofillItemAt(0).getAction());

        verify(mMockAutofillDelegate).suggestionAccepted(0, true);

        assertFalse(getAutofillItemAt(0).isEnabled());
        assertTrue(getAutofillItemAt(0).isLoading());
        assertFalse(getAutofillItemAt(1).isEnabled());
        assertFalse(getAutofillItemAt(1).isLoading());
    }

    @Test
    public void testSuggestionSelectionUsesOriginalIndex() {
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Password Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.PASSWORD_ENTRY)
                        .setOriginalIndex(5)
                        .build();

        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);

        List<ActionBarItem> barItems = flattenItemGroups();
        barItems.get(0).getAction().getCallback().onResult(barItems.get(0).getAction());

        // Verify that suggestionAccepted was called with originalIndex 5 instead of loop index 0.
        verify(mMockAutofillDelegate).suggestionAccepted(5, false);
    }

    @Test
    public void testSuggestionHoverTriggersDelegate() {
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Test Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .build();

        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);

        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.get(0).getAction().getHoverCallback(), notNullValue());

        // Simulate hover enter.
        barItems.get(0).getAction().getHoverCallback().onResult(true);
        verify(mMockAutofillDelegate).suggestionSelectionStateChanged(0, true);

        // Simulate hover exit.
        barItems.get(0).getAction().getHoverCallback().onResult(false);
        verify(mMockAutofillDelegate).suggestionSelectionStateChanged(0, false);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_HOVER_PREVIEW})
    public void testSuggestionHoverDisabledWithoutFlag() {
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Test Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .build();

        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);

        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.get(0).getAction().getHoverCallback(), nullValue());
    }

    private void verifyLongPressOnPersonalContextSuggestionOpensSettings(
            @EntityTypeName int entityTypeName) {
        EntityInstance entityInstance = mock(EntityInstance.class);
        when(entityInstance.getRecordType())
                .thenReturn(
                        org.chromium.components.autofill.autofill_ai.RecordType.PERSONAL_CONTEXT);
        EntityType entityType = mock(EntityType.class);
        when(entityType.getTypeName()).thenReturn(entityTypeName);
        when(entityInstance.getEntityType()).thenReturn(entityType);
        when(mMockEntityDataManager.getEntityInstance("guid")).thenReturn(entityInstance);

        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Personal Context Suggestion")
                        .setSubLabel("Order * Water")
                        .setSuggestionType(SuggestionType.FILL_AUTOFILL_AI)
                        .setFeatureForIph("")
                        .setPayload(new AutofillAiPayload("guid", false))
                        .build();

        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);

        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.get(0).getAction().getLongPressCallback(), notNullValue());

        // Simulate a long press on the suggestion.
        barItems.get(0).getAction().getLongPressCallback().onResult(barItems.get(0).getAction());

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager).showDialog(modelCaptor.capture(), anyInt());
        PropertyModel model = modelCaptor.getValue();
        assertThat(model.get(ModalDialogProperties.TITLE), equalTo("Order * Water"));
        TextView description =
                model.get(ModalDialogProperties.CUSTOM_VIEW)
                        .findViewById(R.id.description_text_view);
        assertThat(
                description.getText().toString(),
                equalTo(
                        ApplicationProvider.getApplicationContext()
                                .getString(
                                        R.string
                                                .autofill_ai_suggestion_long_press_dialog_description)));
        Button positiveButton =
                model.get(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW)
                        .findViewById(R.id.button_primary);
        assertThat(
                positiveButton.getText().toString(),
                equalTo(
                        ApplicationProvider.getApplicationContext()
                                .getString(
                                        R.string
                                                .autofill_ai_suggestion_long_press_dialog_positive_button)));
        Button negativeButton =
                model.get(ModalDialogProperties.CUSTOM_BUTTON_BAR_VIEW)
                        .findViewById(R.id.button_secondary);
        assertThat(
                negativeButton.getText().toString(),
                equalTo(
                        ApplicationProvider.getApplicationContext()
                                .getString(
                                        R.string
                                                .autofill_ai_suggestion_long_press_dialog_negative_button)));
        verify(mMockAutofillDelegate, never()).deleteSuggestion(anyInt());

        model.get(ModalDialogProperties.CONTROLLER)
                .onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mMockAutofillDelegate).openSettingsForEntityType(entityTypeName);
    }

    @Test
    public void testLongPressOnPersonalContextSuggestionOpensDialog() {
        verifyLongPressOnPersonalContextSuggestionOpensSettings(EntityTypeName.PASSPORT);
    }

    @Test
    public void testLongPressOnPersonalContextSuggestionOpensDialog_Travel() {
        verifyLongPressOnPersonalContextSuggestionOpensSettings(EntityTypeName.VEHICLE);
    }

    @Test
    public void testLongPressOnPersonalContextSuggestionOpensDialog_Shopping() {
        verifyLongPressOnPersonalContextSuggestionOpensSettings(EntityTypeName.ORDER);
    }

    @Test
    public void testLongPressOnRegularSuggestionDeletesSuggestion() {
        EntityInstance entityInstance = mock(EntityInstance.class);
        when(entityInstance.getRecordType())
                .thenReturn(org.chromium.components.autofill.autofill_ai.RecordType.LOCAL);
        when(mMockEntityDataManager.getEntityInstance("guid")).thenReturn(entityInstance);

        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Regular Suggestion")
                        .setSecondaryLabel("Order * Water")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .setPayload(new AutofillAiPayload("guid", false))
                        .build();

        mCoordinator.setSuggestions(List.of(suggestion), mMockAutofillDelegate);

        List<ActionBarItem> barItems = flattenItemGroups();
        assertThat(barItems.get(0).getAction().getLongPressCallback(), notNullValue());

        // Simulate a long press on the suggestion.
        barItems.get(0).getAction().getLongPressCallback().onResult(barItems.get(0).getAction());

        ArgumentCaptor<PropertyModel> modelCaptor = ArgumentCaptor.forClass(PropertyModel.class);
        verify(mModalDialogManager, never()).showDialog(modelCaptor.capture(), anyInt());
        verify(mMockAutofillDelegate).deleteSuggestion(0);
    }

    @Test
    public void testSuggestionAcceptanceWithoutLoadingKeepsSuggestionsEnabled() {
        AutofillSuggestion suggestion1 =
                new AutofillSuggestion.Builder()
                        .setLabel("Regular Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .setPayload(new AutofillAiPayload("guid", /* requiresServerFetch= */ false))
                        .build();

        AutofillSuggestion suggestion2 =
                new AutofillSuggestion.Builder()
                        .setLabel("Other Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .setPayload(new AutofillAiPayload("guid", /* requiresServerFetch= */ false))
                        .build();

        mCoordinator.setSuggestions(List.of(suggestion1, suggestion2), mMockAutofillDelegate);

        List<ActionBarItem> barItems = flattenItemGroups();
        assertTrue(barItems.get(0).isEnabled());
        assertTrue(barItems.get(1).isEnabled());

        // Simulate a click on the first suggestion, which does not require loading.
        barItems.get(0).getAction().getCallback().onResult(barItems.get(0).getAction());

        verify(mMockAutofillDelegate).suggestionAccepted(0, false);

        // The suggestions should remain enalbled because no loading UI is shown.
        barItems = flattenItemGroups();
        assertTrue(barItems.get(0).isEnabled());
        assertTrue(barItems.get(1).isEnabled());
    }

    @Test
    public void testSuggestionAcceptanceDisablesSheetOpener() {
        AutofillSuggestion suggestion1 =
                new AutofillSuggestion.Builder()
                        .setLabel("Loading Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .setPayload(new AutofillAiPayload("guid", true))
                        .build();

        mCoordinator.setSuggestions(List.of(suggestion1), mMockAutofillDelegate);

        SheetOpenerBarItem sheetOpener = (SheetOpenerBarItem) mModel.get(SHEET_OPENER_ITEM);
        assertTrue(sheetOpener.isEnabled());

        List<ActionBarItem> barItems = flattenItemGroups();
        // Simulate a click on the first suggestion.
        barItems.get(0).getAction().getCallback().onResult(barItems.get(0).getAction());

        assertFalse(sheetOpener.isEnabled());
    }

    @Test
    public void testDismissEnablesFixedBarItems() {
        mCoordinator.show();

        SheetOpenerBarItem sheetOpener = (SheetOpenerBarItem) mModel.get(SHEET_OPENER_ITEM);
        DismissBarItem dismissItem = (DismissBarItem) mModel.get(DISMISS_ITEM);

        sheetOpener.setEnabled(false);
        dismissItem.setEnabled(false);

        mCoordinator.dismiss();

        assertTrue(sheetOpener.isEnabled());
        assertTrue(dismissItem.isEnabled());
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
                KeyboardAccessoryStyle.createDockedKeyboardAccessoryStyle(/* verticalOffset= */ 1);
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
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)
    @SuppressWarnings("unchecked") // Hamcrest contains(Matcher...) varargs heap pollution.
    public void testAndroidDesktopHasDismissButton() {
        DeviceInfo.setIsDesktopForTesting(true);

        mCoordinator.setSuggestions(List.of(mock(AutofillSuggestion.class)), mMockAutofillDelegate);

        assertThat(mModel.get(BAR_ITEMS), contains(instanceOf(AutofillBarItem.class)));
        assertThat(
                mModel.get(BAR_ITEMS_FIXED),
                contains(instanceOf(SheetOpenerBarItem.class), instanceOf(DismissBarItem.class)));
    }

    @Test
    public void testAndroidDesktopDynamicPositioningHasNoDismissButton() {
        DeviceInfo.setIsDesktopForTesting(true);

        mCoordinator.setSuggestions(List.of(mock(AutofillSuggestion.class)), mMockAutofillDelegate);

        assertThat(mModel.get(BAR_ITEMS), contains(instanceOf(AutofillBarItem.class)));

        assertThat(mModel.get(BAR_ITEMS_FIXED), not(hasItem(instanceOf(DismissBarItem.class))));
        assertThat(mModel.get(BAR_ITEMS_FIXED), contains(instanceOf(SheetOpenerBarItem.class)));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)
    @SuppressWarnings("unchecked") // Hamcrest contains(Matcher...) varargs heap pollution.
    public void testAndroidDesktopHasFixedItems() {
        DeviceInfo.setIsDesktopForTesting(true);
        Provider<Action[]> generationProvider = new Provider<>(GENERATE_PASSWORD_AUTOMATIC);
        mCoordinator.registerActionProvider(generationProvider);
        AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.AUTOCOMPLETE_ENTRY)
                        .setFeatureForIph("")
                        .build();
        Action generationAction =
                new Action(GENERATE_PASSWORD_AUTOMATIC, CallbackUtils.emptyCallback());

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
        final Action generationAction =
                new Action(GENERATE_PASSWORD_AUTOMATIC, CallbackUtils.emptyCallback());
        generationProvider.notifyObservers(new Action[] {generationAction});
        assertThat(mModel.get(BAR_ITEMS).size(), is(6));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(ActionBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(3), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(4), instanceOf(AutofillBarItem.class));
    }

    @Test
    public void testGroupCreationForAutofillAi() {
        final AutofillSuggestion suggestion =
                new AutofillSuggestion.Builder()
                        .setLabel("John Doe")
                        .setSubLabel("Passport")
                        .setSuggestionType(SuggestionType.FILL_AUTOFILL_AI)
                        .setFeatureForIph("")
                        .build();
        mCoordinator.setSuggestions(
                List.of(suggestion, suggestion, suggestion), mMockAutofillDelegate);
        // Autofill AI suggestion width should be limited.
        assertThat(mModel.get(BAR_ITEMS).size(), is(2));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(GroupBarItem.class));
    }

    @Test
    public void testGroupCreationForCreditCards() {
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

        DeviceInfo.setIsDesktopForTesting(true);
        mCoordinator.setStyle(
                KeyboardAccessoryStyle.createUndockedKeyboardAccessoryStyle(
                        /* horizontalOffset= */ 1,
                        /* verticalOffset= */ 1,
                        /* maxWidth= */ 1,
                        KeyboardAccessoryStyle.NotchPosition.TOP));
        // The suggestions should not be grouped because the style was changed to undocked.
        // TODO: crbug.com/431185714 - Mediator should remove the sheet opener when the style is
        // changed to undocked.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));

        DeviceInfo.setIsDesktopForTesting(false);
        mCoordinator.setStyle(
                KeyboardAccessoryStyle.createDockedKeyboardAccessoryStyle(/* verticalOffset= */ 1));
        // The suggestions should be grouped again since the style was changed to docked.
        assertThat(mModel.get(BAR_ITEMS).size(), is(2));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(GroupBarItem.class));
    }

    @Test
    public void testGroupCreationWhenStyleIsUndocked() {
        DeviceInfo.setIsDesktopForTesting(true);

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

        DeviceInfo.setIsDesktopForTesting(true);

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
        final Action credmanAction =
                new Action(CREDMAN_CONDITIONAL_UI_REENTRY, CallbackUtils.emptyCallback());
        credmanActionProvider.notifyObservers(new Action[] {credmanAction});
        // The suggestions should not be grouped because the style was set to undocked.
        assertThat(mModel.get(BAR_ITEMS).size(), is(4));
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(2), instanceOf(AutofillBarItem.class));
    }

    @Test
    public void testSetSelectedSuggestionWithGroupedSuggestions() {
        DeviceInfo.setIsDesktopForTesting(false);

        AutofillSuggestion suggestion1 =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion 1")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setOriginalIndex(0)
                        .build();
        AutofillSuggestion suggestion2 =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion 2")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setOriginalIndex(1)
                        .build();
        AutofillSuggestion suggestion3 =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion 3")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setOriginalIndex(2)
                        .build();
        AutofillSuggestion suggestion4 =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion 4")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setOriginalIndex(3)
                        .build();

        mCoordinator.setSuggestions(
                List.of(suggestion1, suggestion2, suggestion3, suggestion4), mMockAutofillDelegate);

        // First 3 suggestions are grouped, 4th is individual.
        assertThat(mModel.get(BAR_ITEMS).size(), is(3)); // Group + 4th suggestion + tab layout.
        assertThat(mModel.get(BAR_ITEMS).get(0), instanceOf(GroupBarItem.class));
        assertThat(mModel.get(BAR_ITEMS).get(1), instanceOf(AutofillBarItem.class));
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), nullValue());

        // Select first suggestion (inside group).
        mCoordinator.setSelectedSuggestion(0);
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), is(0));
        assertThat(mCoordinator.getSelectedSuggestionForTesting(), is(0));

        // Select second suggestion (inside group).
        mCoordinator.setSelectedSuggestion(1);
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), is(1));
        assertThat(mCoordinator.getSelectedSuggestionForTesting(), is(1));

        // Select fourth suggestion (outside group).
        mCoordinator.setSelectedSuggestion(3);
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), is(3));
        assertThat(mCoordinator.getSelectedSuggestionForTesting(), is(3));

        // Clear suggestion selection.
        mCoordinator.setSelectedSuggestion(null);
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), nullValue());
        assertThat(mCoordinator.getSelectedSuggestionForTesting(), nullValue());
    }

    @Test
    public void testSetSelectedSuggestionWithFilteredSuggestions() {
        AutofillSuggestion addressSuggestion1 =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion 1")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setOriginalIndex(1)
                        .build();
        AutofillSuggestion addressSuggestion2 =
                new AutofillSuggestion.Builder()
                        .setLabel("Suggestion 2")
                        .setSubLabel("")
                        .setSuggestionType(SuggestionType.ADDRESS_ENTRY)
                        .setOriginalIndex(2)
                        .build();

        // Pass suggestions that were filtered by C++ and have original indices [1: Address1, 2:
        // Address2].
        mCoordinator.setSuggestions(
                List.of(addressSuggestion1, addressSuggestion2), mMockAutofillDelegate);

        // Address1 and Address2 should be shown as autofill items (plus the sheet opener).
        assertThat(flattenItemGroups().size(), is(3));
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), nullValue());

        // Select Address1 using its ground-truth index (1 in original suggestions list).
        mCoordinator.setSelectedSuggestion(1);
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), is(1));

        // Select Address2 using its ground-truth index (2 in original suggestions list).
        mCoordinator.setSelectedSuggestion(2);
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), is(2));

        // Selecting a non-visible index (e.g. 0, 3, or 4) updates the model.
        mCoordinator.setSelectedSuggestion(4);
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), is(4));

        // Clear suggestion selection.
        mCoordinator.setSelectedSuggestion(null);
        assertThat(mModel.get(SELECTED_SUGGESTION_INDEX), nullValue());
    }

    @Test
    public void testSetSelectedSuggestionNotifiesObserver() {
        mModel.addObserver(mMockPropertyObserver);
        mCoordinator.show();
        mCoordinator.setSelectedSuggestion(1);
        verify(mMockPropertyObserver).onPropertyChanged(mModel, SELECTED_SUGGESTION_INDEX);

        mCoordinator.setSelectedSuggestion(null);
        verify(mMockPropertyObserver, times(2))
                .onPropertyChanged(mModel, SELECTED_SUGGESTION_INDEX);
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
