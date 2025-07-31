// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_ui.TabSwitcherGroupSuggestionService.SuggestionLifecycleObserver;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.DismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.tab_ui.R;

import java.util.ArrayList;
import java.util.List;

/**
 * A message service to surface a message card that suggests creating a tab group from a selection
 * of tabs.
 */
@NullMarked
public class TabGroupSuggestionMessageService extends MessageService
        implements MessageUpdateObserver {
    /** Callback to start the merge animation which runs upon accepting a suggestion. */
    @FunctionalInterface
    public interface StartMergeAnimation {
        /**
         * Starts the merge animation.
         *
         * @param targetTabId The tab that will serve as the destination for the other tabs.
         * @param tabIdsToShift The tab IDs for the tabs that will merge into the target tab.
         * @param onAnimationEnd Executed after the animation has finished.
         */
        void start(
                @TabId int targetTabId,
                List<@TabId Integer> tabIdsToShift,
                Runnable onAnimationEnd);
    }

    /** This is the data type that this MessageService is serving to its Observer. */
    public static class TabGroupSuggestionMessageData implements MessageData {
        private final int mNumTabs;
        private final Context mContext;
        private final ReviewActionProvider mActionProvider;
        private final DismissActionProvider mDismissActionProvider;

        /**
         * @param numTabs The number of tabs in the suggestion.
         * @param context The context used obtaining the message strings.
         * @param actionProvider The provider for the primary action.
         * @param dismissActionProvider The provider for the dismiss action.
         */
        TabGroupSuggestionMessageData(
                int numTabs,
                Context context,
                ReviewActionProvider actionProvider,
                DismissActionProvider dismissActionProvider) {
            mNumTabs = numTabs;
            mContext = context;
            mActionProvider = actionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        /** The provider for the review action callback. */
        public ReviewActionProvider getReviewActionProvider() {
            return mActionProvider;
        }

        /** The provider for the dismiss action callback. */
        public DismissActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }

        /** The message text to be displayed. */
        public String getMessageText() {
            return mContext.getString(R.string.tab_group_suggestion_message, mNumTabs);
        }

        /** The text for the message card action button. */
        public String getActionText() {
            return mContext.getString(R.string.tab_group_suggestion_message_action_text, mNumTabs);
        }

        /** The dismiss action text for the message card. */
        public String getDismissActionText() {
            return mContext.getString(R.string.no_thanks);
        }
    }

    private final Context mContext;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;

    private final Callback<@TabId Integer> mAddOnMessageAfterTabCallback;
    private final StartMergeAnimation mStartMergeAnimation;
    private boolean mMessageCurrentlyShown;

    /**
     * @param context The context for this service.
     * @param currentTabGroupModelFilterSupplier The supplier for the current {@link
     *     TabGroupModelFilter}.
     * @param onMessageAfterTabCallback A callback to be called to add a message after a tab.
     * @param startMergeAnimation A callback used to start the merge animation.
     */
    public TabGroupSuggestionMessageService(
            Context context,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            Callback<@TabId Integer> onMessageAfterTabCallback,
            StartMergeAnimation startMergeAnimation) {
        super(MessageType.TAB_GROUP_SUGGESTION_MESSAGE);
        mContext = context;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mAddOnMessageAfterTabCallback = onMessageAfterTabCallback;
        mStartMergeAnimation = startMergeAnimation;
    }

    /**
     * Dismisses the suggestion message.
     *
     * @param onDismissMessageListener The runnable to run on dismissing the message.
     */
    public void dismissMessage(Runnable onDismissMessageListener) {
        if (!mMessageCurrentlyShown) return;
        sendInvalidNotification();
        mMessageCurrentlyShown = false;
        onDismissMessageListener.run();
    }

    /**
     * Attempts to show a group suggestion message for a given list of tabs.
     *
     * @param tabIdsSortedByIndex The list of tab IDs to be considered for the group suggestion.
     *     These must be sorted by the associated tab's tab model index.
     * @param responseListener The listener watching user responses to messages.
     */
    public void addGroupMessageForTabs(
            List<@TabId Integer> tabIdsSortedByIndex,
            SuggestionLifecycleObserver responseListener) {
        if (tabIdsSortedByIndex.isEmpty()) return;
        if (mMessageCurrentlyShown) return;

        TabGroupSuggestionMessageData data =
                new TabGroupSuggestionMessageData(
                        tabIdsSortedByIndex.size(),
                        mContext,
                        () -> onAcceptMessage(tabIdsSortedByIndex, responseListener),
                        ignored -> dismissMessage(responseListener::onSuggestionDismissed));
        sendAvailabilityNotification(data);
        mMessageCurrentlyShown = true;

        @TabId int lastTabId = tabIdsSortedByIndex.get(tabIdsSortedByIndex.size() - 1);
        mAddOnMessageAfterTabCallback.onResult(lastTabId);
    }

    private void onAcceptMessage(
            List<@TabId Integer> tabIdsSortedByIndex,
            SuggestionLifecycleObserver responseListener) {
        Runnable onAnimationEnd =
                () -> groupTabs(tabIdsSortedByIndex, responseListener::onSuggestionAccepted);

        int numTabs = tabIdsSortedByIndex.size();
        List<@TabId Integer> shiftedTabIds = new ArrayList<>(numTabs);
        shiftedTabIds.addAll(tabIdsSortedByIndex.subList(1, numTabs));

        if (numTabs > 1) {
            mStartMergeAnimation.start(tabIdsSortedByIndex.get(0), shiftedTabIds, onAnimationEnd);
        }
    }

    private void groupTabs(List<@TabId Integer> tabIds, Runnable onAcceptMessageListener) {
        assert !tabIds.isEmpty();

        onAcceptMessageListener.run();
        TabGroupModelFilter tabGroupModelFilter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(tabGroupModelFilter);
        TabModel tabModel = tabGroupModelFilter.getTabModel();
        List<Tab> tabs = TabModelUtils.getTabsById(tabIds, tabModel, false);

        // Just dismiss message if there are no tabs to group.
        if (!tabs.isEmpty()) {
            Tab tab = tabs.get(0);
            tabGroupModelFilter.mergeListOfTabsToGroup(tabs, tab, /* notify= */ true);
        }

        dismissMessage(CallbackUtils.emptyRunnable());
    }
}
