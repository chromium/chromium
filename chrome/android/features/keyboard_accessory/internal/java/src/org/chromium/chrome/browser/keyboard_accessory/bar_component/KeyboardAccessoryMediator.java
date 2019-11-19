// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BOTTOM_OFFSET_PX;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.KEYBOARD_TOGGLE_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_TITLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_KEYBOARD_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.TAB_LAYOUT_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetTrigger;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator.TabSwitchingDelegate;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator.VisibilityDelegate;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.AutofillBarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BarItem;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.TabLayoutBarItem;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Action;
import org.chromium.chrome.browser.keyboard_accessory.data.Provider;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabLayoutCoordinator;
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.PopupItemId;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * This is the second part of the controller of the keyboard accessory component.
 * It is responsible for updating the model based on backend calls and notify the backend if the
 * model changes. From the backend, it receives all actions that the accessory can perform (most
 * prominently generating passwords) and lets the model know of these actions and which callback to
 * trigger when selecting them.
 */
class KeyboardAccessoryMediator
        implements PropertyObservable.PropertyObserver<PropertyKey>, Provider.Observer<Action[]>,
                   KeyboardAccessoryTabLayoutCoordinator.AccessoryTabObserver {
    private final PropertyModel mModel;
    private final VisibilityDelegate mVisibilityDelegate;
    private final TabSwitchingDelegate mTabSwitcher;

    KeyboardAccessoryMediator(PropertyModel model, VisibilityDelegate visibilityDelegate,
            TabSwitchingDelegate tabSwitcher,
            KeyboardAccessoryTabLayoutCoordinator.TabLayoutCallbacks tabLayoutCallbacks) {
        mModel = model;
        mVisibilityDelegate = visibilityDelegate;
        mTabSwitcher = tabSwitcher;

        // Add mediator as observer so it can use model changes as signal for accessory visibility.
        mModel.set(SHOW_KEYBOARD_CALLBACK, this::closeSheet);
        mModel.set(TAB_LAYOUT_ITEM, new TabLayoutBarItem(tabLayoutCallbacks));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            mModel.get(BAR_ITEMS).add(mModel.get(TAB_LAYOUT_ITEM));
        }
        mModel.addObserver(this);
    }

    /**
     * Creates an observer object that refreshes the accessory bar items when a connected provider
     * notifies it about new {@link AutofillSuggestion}s. It ensures the delegate receives
     * interactions with the view representing a suggestion.
     * @param delegate A {@link AutofillDelegate}.
     * @return A {@link Provider.Observer} accepting only {@link AutofillSuggestion}s.
     */
    Provider.Observer<AutofillSuggestion[]> createAutofillSuggestionsObserver(
            AutofillDelegate delegate) {
        return (@AccessoryAction int typeId, AutofillSuggestion[] suggestions) -> {
            assert typeId
                    == AccessoryAction.AUTOFILL_SUGGESTION
                : "Autofill suggestions observer received wrong data: "
                            + typeId;
            List<BarItem> retainedItems = collectItemsToRetain(AccessoryAction.AUTOFILL_SUGGESTION);
            retainedItems.addAll(toBarItems(suggestions, delegate));
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
                retainedItems.add(retainedItems.size(), mModel.get(TAB_LAYOUT_ITEM));
            }
            mModel.get(BAR_ITEMS).set(retainedItems);
        };
    }

    @Override
    public void onItemAvailable(
            @AccessoryAction int typeId, KeyboardAccessoryData.Action[] actions) {
        assert typeId != DEFAULT_TYPE : "Did not specify which Action type has been updated.";
        List<BarItem> retainedItems = collectItemsToRetain(typeId);
        retainedItems.addAll(0, toBarItems(actions));
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            retainedItems.add(retainedItems.size(), mModel.get(TAB_LAYOUT_ITEM));
        }
        mModel.get(BAR_ITEMS).set(retainedItems);
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
     * @param suggestion This {@link AutofillSuggestion} will be checked for usefulness.
     * @return True iff the suggestion should be displayed.
     */
    private boolean shouldShowSuggestion(AutofillSuggestion suggestion) {
        switch (suggestion.getSuggestionId()) {
            case PopupItemId.ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE:
                // The insecure context warning has a replacement in the fallback sheet.
            case PopupItemId.ITEM_ID_SEPARATOR:
            case PopupItemId.ITEM_ID_CLEAR_FORM:
            case PopupItemId.ITEM_ID_CREDIT_CARD_SIGNIN_PROMO:
            case PopupItemId.ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY:
            case PopupItemId.ITEM_ID_GENERATE_PASSWORD_ENTRY:
            case PopupItemId.ITEM_ID_SHOW_ACCOUNT_CARDS:
            case PopupItemId.ITEM_ID_AUTOFILL_OPTIONS:
                return false;
            case PopupItemId.ITEM_ID_AUTOCOMPLETE_ENTRY:
            case PopupItemId.ITEM_ID_PASSWORD_ENTRY:
            case PopupItemId.ITEM_ID_DATALIST_ENTRY:
            case PopupItemId.ITEM_ID_SCAN_CREDIT_CARD:
            case PopupItemId.ITEM_ID_TITLE:
            case PopupItemId.ITEM_ID_USERNAME_ENTRY:
            case PopupItemId.ITEM_ID_CREATE_HINT:
                return true;
        }
        return true; // If it's not a special id, show the regular suggestion!
    }

    private List<AutofillBarItem> toBarItems(
            AutofillSuggestion[] suggestions, AutofillDelegate delegate) {
        List<AutofillBarItem> barItems = new ArrayList<>(suggestions.length);
        for (int position = 0; position < suggestions.length; ++position) {
            AutofillSuggestion suggestion = suggestions[position];
            if (!shouldShowSuggestion(suggestion)) continue;
            barItems.add(new AutofillBarItem(suggestion, createAutofillAction(delegate, position)));
        }
        return barItems;
    }

    /**
     * Annotates the first suggestion in with an in-product help bubble. For password suggestions,
     * the first suggestion is usually autofilled and therefore, the second element is annotated.
     *
     * This doesn't necessary mean that the IPH bubble will be shown - a final check will be
     * performed right before the bubble can be displayed.
     */
    void prepareUserEducation() {
        ListModel<BarItem> items = mModel.get(BAR_ITEMS);
        boolean skippedFirstPasswordItem = false;
        for (int i = 0; i < items.size(); ++i) {
            if (items.get(i).getViewType() != BarItem.Type.SUGGESTION) continue;
            AutofillBarItem barItem = (AutofillBarItem) items.get(i);
            if (!skippedFirstPasswordItem && containsPasswordInfo(barItem.getSuggestion())) {
                // For password suggestions, we want to educate about the 2nd entry.
                skippedFirstPasswordItem = true;
                continue;
            }
            barItem.setFeatureForIPH(getFeatureBySuggestionId(barItem.getSuggestion()));
            items.update(i, barItem);
            break; // Only set IPH for one suggestions in the bar.
        }
    }

    private Collection<BarItem> toBarItems(Action[] actions) {
        List<BarItem> barItems = new ArrayList<>(actions.length);
        for (Action action : actions) {
            barItems.add(new BarItem(toBarItemType(action.getActionType()), action));
        }
        return barItems;
    }

    private KeyboardAccessoryData.Action createAutofillAction(AutofillDelegate delegate, int pos) {
        return new KeyboardAccessoryData.Action(
                null, // Unused. The AutofillSuggestion has more meaningful labels.
                AccessoryAction.AUTOFILL_SUGGESTION, result -> {
                    ManualFillingMetricsRecorder.recordActionSelected(
                            AccessoryAction.AUTOFILL_SUGGESTION);
                    delegate.suggestionSelected(pos);
                });
    }

    private @BarItem.Type int toBarItemType(@AccessoryAction int accessoryAction) {
        switch (accessoryAction) {
            case AccessoryAction.AUTOFILL_SUGGESTION:
                return BarItem.Type.SUGGESTION;
            case AccessoryAction.GENERATE_PASSWORD_AUTOMATIC:
                return BarItem.Type.ACTION_BUTTON;
            case AccessoryAction.MANAGE_PASSWORDS: // Intentional fallthrough - no view defined.
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
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        // Update the visibility only if we haven't set it just now.
        if (propertyKey == VISIBLE) {
            // When the accessory just (dis)appeared, there should be no active tab.
            mTabSwitcher.closeActiveTab();
            if (!mModel.get(VISIBLE)) {
                // TODO(fhorschig|ioanap): Maybe the generation bridge should take care of that.
                onItemAvailable(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, new Action[0]);
            }
            return;
        }
        if (propertyKey == KEYBOARD_TOGGLE_VISIBLE) {
            KeyboardAccessoryData.Tab activeTab = mTabSwitcher.getActiveTab();
            if (activeTab != null) mModel.set(SHEET_TITLE, activeTab.getTitle());
            return;
        }
        if (propertyKey == BOTTOM_OFFSET_PX || propertyKey == SHOW_KEYBOARD_CALLBACK
                || propertyKey == TAB_LAYOUT_ITEM || propertyKey == SHEET_TITLE
                || propertyKey == SKIP_CLOSING_ANIMATION
                || propertyKey == DISABLE_ANIMATIONS_FOR_TESTING) {
            return;
        }
        assert false : "Every property update needs to be handled explicitly!";
    }

    @Override
    public void onActiveTabChanged(Integer activeTab) {
        mModel.set(KEYBOARD_TOGGLE_VISIBLE, activeTab != null);
        if (activeTab == null) {
            mVisibilityDelegate.onCloseAccessorySheet();
            return;
        }
        mVisibilityDelegate.onChangeAccessorySheet(activeTab);
    }

    @Override
    public void onActiveTabReselected() {
        closeSheet();
    }

    private void closeSheet() {
        assert mTabSwitcher.getActiveTab() != null;
        ManualFillingMetricsRecorder.recordSheetTrigger(
                mTabSwitcher.getActiveTab().getRecordingType(), AccessorySheetTrigger.MANUAL_CLOSE);
        mModel.set(KEYBOARD_TOGGLE_VISIBLE, false);
        mVisibilityDelegate.onCloseAccessorySheet();
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
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            return mModel.get(BAR_ITEMS).size() > 1; // Ignore tab switcher item.
        }
        return mModel.get(BAR_ITEMS).size() > 0;
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

    @VisibleForTesting
    PropertyModel getModelForTesting() {
        return mModel;
    }

    private static String getFeatureBySuggestionId(AutofillSuggestion suggestion) {
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
        return suggestion.getSuggestionId() == PopupItemId.ITEM_ID_USERNAME_ENTRY
                || suggestion.getSuggestionId() == PopupItemId.ITEM_ID_PASSWORD_ENTRY;
    }

    private static boolean containsCreditCardInfo(AutofillSuggestion suggestion) {
        return suggestion.getSuggestionId() > 0 && (suggestion.getSuggestionId() & 0xFFFF0000) != 0;
    }

    private static boolean containsAddressInfo(AutofillSuggestion suggestion) {
        return suggestion.getSuggestionId() > 0 && (suggestion.getSuggestionId() & 0x0000FFFF) != 0;
    }
}
