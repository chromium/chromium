// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATE_SUGGESTIONS_FROM_TOP;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ANIMATION_LISTENER;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.BAR_ITEMS_FIXED;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISABLE_ANIMATIONS_FOR_TESTING;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.DISMISS_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_STICKY_LAST_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.HAS_SUGGESTIONS;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.OBFUSCATED_CHILD_AT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.ON_TOUCH_EVENT_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHEET_OPENER_ITEM;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SHOW_SWIPING_IPH;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.SKIP_CLOSING_ANIMATION;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.STYLE;
import static org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryProperties.VISIBLE;

import androidx.annotation.StringRes;

import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryAction;
import org.chromium.chrome.browser.keyboard_accessory.KeyboardAccessoryVisualStateProvider;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator.BarVisibilityDelegate;
import org.chromium.chrome.browser.keyboard_accessory.bar_component.KeyboardAccessoryCoordinator.TabSwitchingDelegate;
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
import org.chromium.components.autofill.AutofillDelegate;
import org.chromium.components.autofill.AutofillSuggestion;
import org.chromium.components.autofill.FillingProduct;
import org.chromium.components.autofill.FillingProductBridge;
import org.chromium.components.autofill.SuggestionType;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.function.Supplier;

/**
 * This is the second part of the controller of the keyboard accessory component. It is responsible
 * for updating the model based on backend calls and notify the backend if the model changes. From
 * the backend, it receives all actions that the accessory can perform (most prominently generating
 * passwords) and lets the model know of these actions and which callback to trigger when selecting
 * them.
 */
@NullMarked
class KeyboardAccessoryMediator
        implements PropertyObservable.PropertyObserver<PropertyKey>,
                Provider.Observer<Action[]>,
                KeyboardAccessoryButtonGroupCoordinator.AccessoryTabObserver {
    private final PropertyModel mModel;
    private final BarVisibilityDelegate mBarVisibilityDelegate;
    private final AccessorySheetCoordinator.SheetVisibilityDelegate mSheetVisibilityDelegate;
    private final TabSwitchingDelegate mTabSwitcher;
    private final Supplier<Integer> mBackgroundColorSupplier;
    private final Supplier<Boolean> mIsLargeFormFactorSupplier;
    private final Profile mProfile;
    private @Nullable Boolean mHasFilteredTouchEvent;
    private final ObserverList<KeyboardAccessoryVisualStateProvider.Observer> mVisualObservers =
            new ObserverList<>();

    KeyboardAccessoryMediator(
            PropertyModel model,
            Profile profile,
            BarVisibilityDelegate barVisibilityDelegate,
            AccessorySheetCoordinator.SheetVisibilityDelegate sheetVisibilityDelegate,
            TabSwitchingDelegate tabSwitcher,
            KeyboardAccessoryButtonGroupCoordinator.SheetOpenerCallbacks sheetOpenerCallbacks,
            Supplier<Integer> backgroundColorSupplier,
            Supplier<Boolean> isLargeFormFactorSupplier,
            Runnable dismissRunnable) {
        mModel = model;
        mProfile = profile;
        mBarVisibilityDelegate = barVisibilityDelegate;
        mSheetVisibilityDelegate = sheetVisibilityDelegate;
        mTabSwitcher = tabSwitcher;
        mBackgroundColorSupplier = backgroundColorSupplier;
        mIsLargeFormFactorSupplier = isLargeFormFactorSupplier;

        // Add mediator as observer so it can use model changes as signal for accessory visibility.
        mModel.set(OBFUSCATED_CHILD_AT_CALLBACK, this::onSuggestionObfuscatedAt);
        mModel.set(ON_TOUCH_EVENT_CALLBACK, this::onTouchEvent);
        mModel.set(SHEET_OPENER_ITEM, new SheetOpenerBarItem(sheetOpenerCallbacks));
        mModel.set(DISMISS_ITEM, new DismissBarItem(dismissRunnable));
        mModel.set(ANIMATION_LISTENER, mBarVisibilityDelegate::onBarFadeInAnimationEnd);
        mModel.get(BAR_ITEMS).add(mModel.get(SHEET_OPENER_ITEM));
        mModel.addObserver(this);
    }

    /**
     * Replaces existing {@link AutofillSuggestion} items in the accessory bar with a new list.
     *
     * <p>This method retains any existing non-suggestion items, converts the new suggestions into
     * bar items (using the delegate for interactions), and re-adds standard items like the sheet
     * opener and dismiss button (if applicable). Finally, it updates the model.
     *
     * @param suggestions The new list of {@link AutofillSuggestion}s to display.
     * @param delegate The {@link AutofillDelegate} to handle interactions with the new suggestion
     *     views.
     */
    void setSuggestions(List<AutofillSuggestion> suggestions, AutofillDelegate delegate) {
        List<BarItem> retainedItems = collectItemsToRetain(AccessoryAction.AUTOFILL_SUGGESTION);
        retainedItems.addAll(toBarItems(suggestions, delegate));
        setBarContents(retainedItems);
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
                        || typeId == AccessoryAction.RETRIEVE_TRUSTED_VAULT_KEY
                : "Did not specify which Action type has been updated.";
        List<BarItem> retainedItems = collectItemsToRetain(typeId);
        retainedItems.addAll(
                typeId == AccessoryAction.CREDMAN_CONDITIONAL_UI_REENTRY ? retainedItems.size() : 0,
                toBarItems(actions));
        setBarContents(retainedItems);
        TraceEvent.end("KeyboardAccessoryMediator#onItemAvailable");
    }

    /**
     * Sets the contents of the accessory bar.
     *
     * <p>This method updates the fixed items, adds the sheet opener to the scrollable items if not
     * present in the fixed ones, and then sets the final scrollable item list on the model,
     * updating the suggestion state.
     *
     * @param scrollableItems The list of {@link BarItem}s to be set on the scrollable part of the
     *     bar.
     */
    private void setBarContents(List<BarItem> scrollableItems) {
        // Chip width limiting works when the keyboard accessory spans the whole screen width. Thus
        // the chip group is created only for the docked keyboard accessory. If the suggestions are
        // not grouped initially and then grouped when the STYLE is set, it can cause a UI glitch.
        // However, the manual filling component's STYLE property is updated when the component is
        // shown, so it's not possible.
        if (shouldLimitSuggestionWidth()) {
            // Create chip group to limit chip width only when the keyboard accessory style is set
            // to docked.
            scrollableItems = createGroupBarItem(scrollableItems);
        }
        // TODO(crbug.com/441006939): Show dismiss on first launch too.
        List<BarItem> fixedBarItems = new ArrayList<BarItem>();
        if (showFloatingKeyboardAccessory()) {
            fixedBarItems.add(mModel.get(SHEET_OPENER_ITEM));
            fixedBarItems.add(mModel.get(DISMISS_ITEM));
        } else {
            scrollableItems.add(mModel.get(SHEET_OPENER_ITEM));
        }
        mModel.get(BAR_ITEMS_FIXED).set(fixedBarItems);
        mModel.get(BAR_ITEMS).set(scrollableItems);
        mModel.set(HAS_SUGGESTIONS, barHasSuggestions());
    }

    private List<BarItem> createGroupBarItem(Iterable<BarItem> scrollableItemsIterable) {
        List<BarItem> scrollableItems = new ArrayList<>();
        for (BarItem item : scrollableItemsIterable) {
            scrollableItems.add(item);
        }
        List<ActionBarItem> autofillBarItems = new ArrayList<>();
        // Collect at most 3 Autofill address suggestions that are in the beginning of the list.
        for (int i = 0; i < scrollableItems.size() && autofillBarItems.size() < 3; i++) {
            if (scrollableItems.get(i) instanceof AutofillBarItem autofillBarItem
                    && canLimitWidth(autofillBarItem.getSuggestion().getSuggestionType())) {
                autofillBarItems.add(autofillBarItem);
            } else {
                // Stop collection Autofill suggestions when the first item of a different type is
                // encountered.
                break;
            }
        }
        // If there are not enough Autofill suggestions in the beginning of the list, do not create
        // a chip group for chip width adjustment.
        if (autofillBarItems.size() < 2) {
            return scrollableItems;
        }
        // Otherwise, substitute the first Autofill suggestions with a suggestion group to
        // dynamically limit their screen width in the Keyboard Accessory.
        scrollableItems.removeAll(autofillBarItems);
        GroupBarItem groupBarItem = new GroupBarItem(autofillBarItems);
        scrollableItems.add(0, groupBarItem);
        return scrollableItems;
    }

    private List<BarItem> ungroupBarItems(Iterable<BarItem> scrollableItems) {
        List<BarItem> barItems = new ArrayList<>();
        for (BarItem barItem : scrollableItems) {
            if (barItem instanceof GroupBarItem) {
                barItems.addAll(barItem.getActionBarItems());
            } else {
                barItems.add(barItem);
            }
        }
        return barItems;
    }

    private List<BarItem> collectItemsToRetain(@AccessoryAction int actionType) {
        List<BarItem> retainedItems = new ArrayList<>();
        // Fallback sheet menu and dismiss button are never retained.
        for (BarItem item : mModel.get(BAR_ITEMS)) {
            for (ActionBarItem actionItem : item.getActionBarItems()) {
                if (actionItem.getAction() == null) continue;
                if (actionItem.getAction().getActionType() == AccessoryAction.DISMISS) continue;
                if (actionItem.getAction().getActionType() == actionType) continue;
                retainedItems.add(actionItem);
            }
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
            case SuggestionType.ALL_LOYALTY_CARDS_ENTRY:
            case SuggestionType.ALL_SAVED_PASSWORDS_ENTRY:
            case SuggestionType.GENERATE_PASSWORD_ENTRY:
            case SuggestionType.MANAGE_ADDRESS:
            case SuggestionType.MANAGE_CREDIT_CARD:
            case SuggestionType.MANAGE_IBAN:
            case SuggestionType.MANAGE_PLUS_ADDRESS:
            case SuggestionType.MANAGE_LOYALTY_CARD:
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
            barItems.add(
                    new AutofillBarItem(
                            suggestion, createAutofillAction(delegate, position), mProfile));
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
            barItem.setFeatureForIph(getFeatureBySuggestionId(barItem.getSuggestion()));
            break; // Only set IPH for one suggestions in the bar.
        }

        return barItems;
    }

    private Collection<ActionBarItem> toBarItems(Action[] actions) {
        List<ActionBarItem> barItems = new ArrayList<>(actions.length);
        for (Action action : actions) {
            barItems.add(
                    new ActionBarItem(
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
            case AccessoryAction.RETRIEVE_TRUSTED_VAULT_KEY:
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
        if (!(mHasFilteredTouchEvent == null || mHasFilteredTouchEvent)) {
            // Log the metric if the accessory received touch events, but none of them were
            // filtered.
            ManualFillingMetricsRecorder.recordHasFilteredTouchEvents(false);
        }
        mHasFilteredTouchEvent = null;
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
                // TODO: crbug.com/398065928 - The generation controller should control the timing..
                onItemAvailable(AccessoryAction.GENERATE_PASSWORD_AUTOMATIC, new Action[0]);
            }
            for (KeyboardAccessoryVisualStateProvider.Observer observer : mVisualObservers) {
                observer.onKeyboardAccessoryVisualStateChanged(
                        mModel.get(VISIBLE), mBackgroundColorSupplier.get());
            }
            return;
        }
        if (propertyKey == STYLE
                || propertyKey == SHEET_OPENER_ITEM
                || propertyKey == DISMISS_ITEM
                || propertyKey == SKIP_CLOSING_ANIMATION
                || propertyKey == DISABLE_ANIMATIONS_FOR_TESTING
                || propertyKey == OBFUSCATED_CHILD_AT_CALLBACK
                || propertyKey == SHOW_SWIPING_IPH
                || propertyKey == HAS_SUGGESTIONS
                || propertyKey == HAS_STICKY_LAST_ITEM
                || propertyKey == ANIMATE_SUGGESTIONS_FROM_TOP
                || propertyKey == ANIMATION_LISTENER
                || propertyKey == BAR_ITEMS_FIXED) {
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
            if (mHasFilteredTouchEvent == null) {
                mHasFilteredTouchEvent = false;
            }
            return;
        }
        if (mHasFilteredTouchEvent == null || !mHasFilteredTouchEvent) {
            // Log the metric if none of the previous touch events were filtered.
            ManualFillingMetricsRecorder.recordHasFilteredTouchEvents(true);
        }
        mHasFilteredTouchEvent = true;
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

    void setStyle(KeyboardAccessoryStyle style) {
        mModel.set(STYLE, style);
        if (style.isDocked()) {
            mModel.get(BAR_ITEMS).set(createGroupBarItem(mModel.get(BAR_ITEMS)));
        } else {
            mModel.get(BAR_ITEMS).set(ungroupBarItems(mModel.get(BAR_ITEMS)));
        }
    }

    void setHasStickyLastItem(boolean hasStickyLastItem) {
        mModel.set(HAS_STICKY_LAST_ITEM, hasStickyLastItem);
    }

    void setAnimateSuggestionsFromTop(boolean animateSuggestionsFromTop) {
        mModel.set(ANIMATE_SUGGESTIONS_FROM_TOP, animateSuggestionsFromTop);
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

    private static @Nullable String getFeatureBySuggestionId(AutofillSuggestion suggestion) {
        // If the suggestion has an explicit IPH feature defined, prefer that over the default IPH
        // features.
        if (suggestion.getFeatureForIph() != null && !suggestion.getFeatureForIph().isEmpty()) {
            return suggestion.getFeatureForIph();
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

    /**
     * Width limiting is applied only to address suggestions.
     *
     * @param suggestionType the type of the displayed suggestion.
     * @return whether the width of the suggestion is allowed to be limited.
     */
    private static boolean canLimitWidth(@SuggestionType int suggestionType) {
        return FillingProductBridge.getFillingProductFromSuggestionType(suggestionType)
                == FillingProduct.ADDRESS;
    }

    private @StringRes int getCaptionId(@AccessoryAction int actionType) {
        switch (actionType) {
            case AccessoryAction.GENERATE_PASSWORD_AUTOMATIC:
                return R.string.password_generation_accessory_button;
            case AccessoryAction.RETRIEVE_TRUSTED_VAULT_KEY:
                return R.string.retrieve_trusted_vault_key_button;
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
        for (var barItem : mModel.get(BAR_ITEMS)) {
            if (barItem.getViewType() == BarItem.Type.SUGGESTION
                    && ((AutofillBarItem) barItem).getSuggestion().getSuggestionType()
                            == SuggestionType.WEBAUTHN_CREDENTIAL) {
                return R.string.more_passkeys;
            }
        }
        return R.string.select_passkey;
    }

    private boolean showFloatingKeyboardAccessory() {
        return mIsLargeFormFactorSupplier.get()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ANDROID_DESKTOP_KEYBOARD_ACCESSORY_REVAMP);
    }

    private boolean shouldLimitSuggestionWidth() {
        return !showFloatingKeyboardAccessory()
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_REDESIGN)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_KEYBOARD_ACCESSORY_CHIP_WIDTH_ADJUSTMENT);
    }

    void addObserver(KeyboardAccessoryVisualStateProvider.Observer observer) {
        mVisualObservers.addObserver(observer);
        observer.onKeyboardAccessoryVisualStateChanged(
                mModel.get(VISIBLE), mBackgroundColorSupplier.get());
    }

    void removeObserver(KeyboardAccessoryVisualStateProvider.Observer observer) {
        mVisualObservers.removeObserver(observer);
    }
}
