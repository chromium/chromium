// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATION_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BOTTOM_OFFSET_PX;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ON_TOUCH_EVENT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator.BarVisibilityDelegate;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator.TabSwitchingDelegate;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SheetOpenerBarItem;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupCoordinator;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetCoordinator;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.function.Predicate;
import java.util.stream.StreamSupport;

/**
 * This is the second part of the controller of the keyboard accessory component. It is responsible
 * for updating the model based on backend calls and notify the backend if the model changes. From
 * the backend, it receives all actions that the accessory can perform (most prominently generating
 * passwords) and lets the model know of these actions and which callback to trigger when selecting
 * them.
 */
class KeyboardAccessoryMediator
        implements PropertyObservable.PropertyObserver<PropertyKey>,
                Provider.Observer<Action[]>,
                KeyboardAccessoryButtonGroupCoordinator.AccessoryTabObserver {
    private final PropertyModel mModel;
    private final BarVisibilityDelegate mBarVisibilityDelegate;
    private final AccessorySheetCoordinator.SheetVisibilityDelegate mSheetVisibilityDelegate;
    private final TabSwitchingDelegate mTabSwitcher;
    private Optional<Boolean> mHasFilteredTouchEvent = Optional.empty();

    KeyboardAccessoryMediator(
            PropertyModel model,
            BarVisibilityDelegate barVisibilityDelegate,
            AccessorySheetCoordinator.SheetVisibilityDelegate sheetVisibilityDelegate,
            TabSwitchingDelegate tabSwitcher,
            KeyboardAccessoryButtonGroupCoordinator.SheetOpenerCallbacks sheetOpenerCallbacks) {
        mModel = model;
        mBarVisibilityDelegate = barVisibilityDelegate;
        mSheetVisibilityDelegate = sheetVisibilityDelegate;
        mTabSwitcher = tabSwitcher;

        // Add mediator as observer so it can use model changes as signal for accessory visibility.
        mModel.set(OBFUSCATED_CHILD_AT_CALLBACK, this::onSuggestionObfuscatedAt);
        mModel.set(ON_TOUCH_EVENT_CALLBACK, this::onTouchEvent);
        mModel.set(SHEET_OPENER_ITEM, new SheetOpenerBarItem(sheetOpenerCallbacks));
        mModel.set(ANIMATION_LISTENER, mBarVisibilityDelegate::onBarFadeInAnimationEnd);
        mModel.get(BAR_ITEMS).add(mModel.get(SHEET_OPENER_ITEM));
        mModel.addObserver(this);
    }

    /**
     * Creates an observer object that refreshes the accessory bar items when a connected provider
     * notifies it about new {@link AutofillSuggestion}s. It ensures the delegate receives
     * interactions with the view representing a suggestion.
     *
     * @param delegate A {@link AutofillDelegate}.
     * @return A {@link Provider.Observer} accepting only {@link AutofillSuggestion}s.
     */
    Provider.Observer<List<AutofillSuggestion>> createAutofillSuggestionsObserver(
            AutofillDelegate delegate) {
        return (@AccessoryAction int typeId, List<AutofillSuggestion> suggestions) -> {
            assert typeId == AccessoryAction.AUTOFILL_SUGGESTION
                    : "Autofill suggestions observer received wrong data: " + typeId;
            List<BarItem> retainedItems = collectItemsToRetain(AccessoryAction.AUTOFILL_SUGGESTION);
            retainedItems.addAll(toBarItems(suggestions, delegate));
            retainedItems.add(retainedItems.size(), mModel.get(SHEET_OPENER_ITEM));
            mModel.get(BAR_ITEMS).set(retainedItems);
            mModel.set(HAS_SUGGESTIONS, barHasSuggestions());
        };
    }

    private boolean barHasSuggestions() {
        for (BarItem barItem : mModel.get(BAR_ITEMS)) {
            if (barItem.getViewType() == BarItem.Type.SUGGESTION) {
                return true;
            }
        }
        return false;
    }

    @Override
    public void onItemAvailable(
            @AccessoryAction int typeId, KeyboardAccessoryData.Action[] actions) {
        TraceEvent.begin("KeyboardAccessoryMediator#onItemAvailable");
        assert typeId == AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY
                        || typeId == AccessoryAction.GENERATE_PASSWORD_AUTOMATIC
                : "Did not specify which Action type has been updated.";
        List<BarItem> retainedItems = collectItemsToRetain(typeId);
        retainedItems.addAll(
                typeId == AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY ? retainedItems.size() : 0,
                toBarItems(actions));
        retainedItems.add(retainedItems.size(), mModel.get(SHEET_OPENER_ITEM));
        mModel.get(BAR_ITEMS).set(retainedItems);
        mModel.set(HAS_SUGGESTIONS, barHasSuggestions());
        TraceEvent.end("KeyboardAccessoryMediator#onItemAvailable");
    }

    private List<BarItem> collectItemsToRetain(@AccessoryAction int actionType) {
        List<BarItem> retainedItems = new ArrayList<>();
        for (BarItem item : mModel.get(BAR_ITEMS)) {
            if (item.getAction() == null) continue;
            if (item.getAction().getActionType() == actionType) continue;
            retainedItems.add(item);
        }
        return retainedItems;
    }

    /**
     * Next to the regular suggestion that we always want to show, there is a number of special
     * suggestions which we want to suppress (e.g. replaced entry points, old warnings, separators).
     *
     * @param suggestion This {@link AutofillSuggestion} will be checked for usefulness.
     * @return True iff the suggestion should be displayed.
     */
    private boolean shouldShowSuggestion(AutofillSuggestion suggestion) {
        switch (suggestion.getSuggestionType()) {
            case SuggestionType.INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
                // The insecure context warning has a replacement in the fallback sheet.
            case SuggestionType.TITLE:
            case SuggestionType.SEPARATOR:
            case SuggestionType.UNDO_OR_CLEAR:
            case SuggestionType.ALL_SAVED_PASSWORDS_ENTRY:
            case SuggestionType.GENERATE_PASSWORD_ENTRY:
            case SuggestionType.SHOW_ACCOUNT_CARDS:
            case SuggestionType.MANAGE_ADDRESS:
            case SuggestionType.MANAGE_CREDIT_CARD:
            case SuggestionType.MANAGE_IBAN:
            case SuggestionType.MANAGE_PLUS_ADDRESS:
                return false;
            case SuggestionType.AUTOCOMPLETE_ENTRY:
            case SuggestionType.PASSWORD_ENTRY:
            case SuggestionType.DATALIST_ENTRY:
            case SuggestionType.SCAN_CREDIT_CARD:
            case SuggestionType.ACCOUNT_STORAGE_PASSWORD_ENTRY:
                return true;
        }
        return true; // If it's not a special id, show the regular suggestion!
    }

    private List<AutofillBarItem> toBarItems(
            List<AutofillSuggestion> suggestions, AutofillDelegate delegate) {
        List<AutofillBarItem> barItems = new ArrayList<>(suggestions.size());
        for (int position = 0; position < suggestions.size(); ++position) {
            AutofillSuggestion suggestion = suggestions.get(position);
            if (!shouldShowSuggestion(suggestion)) continue;
            barItems.add(new AutofillBarItem(suggestion, createAutofillAction(delegate, position)));
        }

        // Annotates the first suggestion in with an in-product help bubble. For password
        // suggestions, the first suggestion is usually autofilled and therefore, the second
        // element is annotated.
        // This doesn't necessary mean that the IPH bubble will be shown - a final check will be
        // performed right before the bubble can be displayed.
        boolean skippedFirstPasswordItem = false;
        for (AutofillBarItem barItem : barItems) {
            if (!skippedFirstPasswordItem && containsPasswordInfo(barItem.getSuggestion())) {
                // For password suggestions, we want to educate about the 2nd entry.
                skippedFirstPasswordItem = true;
                continue;
            }
            barItem.setFeatureForIPH(getFeatureBySuggestionId(barItem.getSuggestion()));
            break; // Only set IPH for one suggestions in the bar.
        }

        return barItems;
    }

    private Collection<BarItem> toBarItems(Action[] actions) {
        List<BarItem> barItems = new ArrayList<>(actions.length);
        for (Action action : actions) {
            barItems.add(
                    new BarItem(
                            toBarItemType(action.getActionType()),
                            action,
                            getCaptionId(action.getActionType())));
        }
        return barItems;
    }

    private Action createAutofillAction(AutofillDelegate delegate, int pos) {
        return new Action(
                AccessoryAction.AUTOFILL_SUGGESTION,
                result -> {
                    ManualFillingMetricsRecorder.recordActionSelected(
                            AccessoryAction.AUTOFILL_SUGGESTION);
                    delegate.suggestionSelected(pos);
                },
                result -> delegate.deleteSuggestion(pos));
    }

    private @BarItem.Type int toBarItemType(@AccessoryAction int accessoryAction) {
        switch (accessoryAction) {
            case AccessoryAction.AUTOFILL_SUGGESTION:
                return BarItem.Type.SUGGESTION;
            case AccessoryAction.GENERATE_PASSWORD_AUTOMATIC:
                return BarItem.Type.ACTION_BUTTON;
            case AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY:
                return BarItem.Type.ACTION_CHIP;
            case AccessoryAction.MANAGE_PASSWORDS: // Intentional fallthrough - no view defined.
            case AccessoryAction.CROSS_DEVICE_PASSKEY:
            case AccessoryAction.COUNT:
                throw new IllegalArgumentException("No view defined for :" + accessoryAction);
        }
        throw new IllegalArgumentException("Unhandled action type:" + accessoryAction);
    }

    void show() {
        mModel.set(SKIP_CLOSING_ANIMATION, false);
        mModel.set(VISIBLE, true);
    }

    void skipClosingAnimationOnce() {
        mModel.set(SKIP_CLOSING_ANIMATION, true);
    }

    void dismiss() {
        mTabSwitcher.closeActiveTab();
        mModel.set(VISIBLE, false);
        if (!mHasFilteredTouchEvent.orElse(true)) {
            // Log the metric if the accessory received touch events, but none of them were
            // filtered.
            ManualFillingMetricsRecorder.recordHasFilteredTouchEvents(false);
        }
        mHasFilteredTouchEvent = Optional.empty();
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        // Update the visibility only if we haven't set it just now.
        if (propertyKey == VISIBLE) {
            mModel.set(SHOW_SWIPING_IPH, false); // Reset IPH if visibility changes.
            // When the accessory just (dis)appeared, there should be no active tab.
            mTabSwitcher.closeActiveTab();
            if (!mModel.get(VISIBLE)) {
                // TODO(fhorschig|ioanap): Maybe the generation bridge should take care of that.
                onItemAvailable(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, new Action[0]);
            }
            return;
        }
        if (propertyKey == BOTTOM_OFFSET_PX
                || propertyKey == SHEET_OPENER_ITEM
                || propertyKey == SKIP_CLOSING_ANIMATION
                || propertyKey == DISABLE_ANIMATIONS_FOR_TESTING
                || propertyKey == OBFUSCATED_CHILD_AT_CALLBACK
                || propertyKey == SHOW_SWIPING_IPH
                || propertyKey == HAS_SUGGESTIONS
                || propertyKey == ANIMATION_LISTENER) {
            return;
        }
        assert false : "Every property update needs to be handled explicitly!";
    }

    @Override
    public void onActiveTabChanged(Integer activeTab) {
        if (activeTab == null) {
            return;
        }
        mSheetVisibilityDelegate.onChangeAccessorySheet(activeTab);
    }

    private void onSuggestionObfuscatedAt(Integer indexOfLast) {
        // Show IPH if at least one entire item (suggestion or fallback) can be revealed by swiping.
        mModel.set(SHOW_SWIPING_IPH, indexOfLast <= mModel.get(BAR_ITEMS).size() - 2);
    }

    private void onTouchEvent(boolean eventFiltered) {
        if (!eventFiltered) {
            mHasFilteredTouchEvent = Optional.of(mHasFilteredTouchEvent.orElse(false));
            return;
        }
        if (!mHasFilteredTouchEvent.orElse(false)) {
            // Log the metric if none of the previous touch events were filtered.
            ManualFillingMetricsRecorder.recordHasFilteredTouchEvents(true);
        }
        mHasFilteredTouchEvent = Optional.of(true);
    }

    /**
     * @return True if neither suggestions nor tabs are available.
     */
    boolean empty() {
        return !hasSuggestions() && !mTabSwitcher.hasTabs();
    }

    /**
     * @return True if the bar contains any suggestions next to the tabs.
     */
    private boolean hasSuggestions() {
        return mModel.get(BAR_ITEMS).size() > 1; // Ignore tab switcher item.
    }

    void setBottomOffset(@Px int bottomOffset) {
        mModel.set(BOTTOM_OFFSET_PX, bottomOffset);
    }

    boolean isShown() {
        return mModel.get(VISIBLE);
    }

    boolean hasActiveTab() {
        return mModel.get(VISIBLE) && mTabSwitcher.getActiveTab() != null;
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    private static String getFeatureBySuggestionId(AutofillSuggestion suggestion) {
        // If the suggestion has an explicit IPH feature defined, prefer that over the default IPH
        // features.
        if (suggestion.getFeatureForIPH() != null && !suggestion.getFeatureForIPH().isEmpty()) {
            return suggestion.getFeatureForIPH();
        }
        if (containsPasswordInfo(suggestion)) {
            return FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE;
        }
        if (containsCreditCardInfo(suggestion)) {
            return FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE;
        }
        if (containsAddressInfo(suggestion)) {
            return FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE;
        }
        return null;
    }

    private static boolean containsPasswordInfo(AutofillSuggestion suggestion) {
        return suggestion.getSuggestionType() == SuggestionType.PASSWORD_ENTRY;
    }

    private static boolean containsCreditCardInfo(AutofillSuggestion suggestion) {
        return suggestion.getSuggestionType() == SuggestionType.CREDIT_CARD_ENTRY;
    }

    private static boolean containsAddressInfo(AutofillSuggestion suggestion) {
        return suggestion.getSuggestionType() == SuggestionType.ADDRESS_ENTRY;
    }

    private @StringRes int getCaptionId(@AccessoryAction int actionType) {
        switch (actionType) {
            case AccessoryAction.GENERATE_PASSWORD_AUTOMATIC:
                return R.string.password_generation_accessory_button;
            case AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY:
                return getCaptionIdForCredManEntry();
            case AccessoryAction.AUTOFILL_SUGGESTION:
            case AccessoryAction.COUNT:
            case AccessoryAction.TOGGLE_SAVE_PASSWORDS:
            case AccessoryAction.USE_OTHER_PASSWORD:
            case AccessoryAction.GENERATE_PASSWORD_MANUAL:
            case AccessoryAction.MANAGE_ADDRESSES:
            case AccessoryAction.MANAGE_CREDIT_CARDS:
            case AccessoryAction.MANAGE_PASSWORDS:
            case AccessoryAction.CROSS_DEVICE_PASSKEY:
                assert false : "No caption defined for accessory action: " + actionType;
        }
        assert false : "Define a title for accessory action: " + actionType;
        return 0;
    }

    private @StringRes int getCaptionIdForCredManEntry() {
        Predicate<BarItem> hasWebAuthnCredential =
                barItem ->
                        barItem.getViewType() == BarItem.Type.SUGGESTION
                                && ((AutofillBarItem) barItem).getSuggestion().getSuggestionType()
                                        == SuggestionType.WEBAUTHN_CREDENTIAL;
        return StreamSupport.stream(mModel.get(BAR_ITEMS).spliterator(), true)
                        .anyMatch(hasWebAuthnCredential)
                ? R.string.more_passkeys
                : R.string.select_passkey;
    }
}
