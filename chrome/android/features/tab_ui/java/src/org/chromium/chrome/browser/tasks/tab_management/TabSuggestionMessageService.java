// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.ALL_KEYS;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.ARCHIVED_TABS_EXPAND_CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.ARCHIVED_TAB_COUNT;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.DECLUTTER_INFO_TEXT;
import static org.chromium.chrome.browser.tasks.tab_management.DeclutterMessageCardViewProperties.DECLUTTER_SETTINGS_CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.ACCEPTED;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.DISMISSED;
import static org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback.TabSuggestionResponse.NOT_CONSIDERED;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabContext;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestion;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionFeedback;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsObserver;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * One of the concrete {@link MessageService} that only serve {@link MessageType#TAB_SUGGESTION}.
 */
public class TabSuggestionMessageService extends MessageService
        implements TabSuggestionsObserver, CustomMessageCardProvider {
    private static boolean sSuggestionAvailableForTesting;

    /** This is the data type that this MessageService is serving to its Observer. */
    public class TabSuggestionMessageData implements MessageData {
        private final TabSuggestion mTabSuggestion;
        private final Callback<TabSuggestionFeedback> mTabSuggestionFeedback;
        private CustomMessageCardProvider mCustomMessageCardProvider;

        public TabSuggestionMessageData(
                TabSuggestion tabSuggestion,
                Callback<TabSuggestionFeedback> feedbackCallback,
                CustomMessageCardProvider customMessageCardProvider) {
            mTabSuggestion = tabSuggestion;
            mTabSuggestionFeedback = feedbackCallback;
            mCustomMessageCardProvider = customMessageCardProvider;
        }

        public View getView() {
            return mCustomMessageCardProvider.getCustomView();
        }

        /**
         * @return The suggested tabs count.
         */
        public int getSize() {
            return mTabSuggestion.getTabsInfo().size();
        }

        /**
         * @return The suggested action type.
         */
        @TabSuggestion.TabSuggestionAction
        public int getActionType() {
            return mTabSuggestion.getAction();
        }

        /**
         * @return The {@link MessageCardView.ReviewActionProvider} for the associated
         *         {@link TabSuggestion}.
         */
        public MessageCardView.ReviewActionProvider getReviewActionProvider() {
            return new MessageCardView.ReviewActionProvider() {
                @Override
                public void review() {
                    TabSuggestionMessageService.this.review(mTabSuggestion, mTabSuggestionFeedback);
                }
            };
        }

        /**
         * @return The {@link MessageCardView.DismissActionProvider} for the associated
         *         {@link TabSuggestion}.
         */
        public MessageCardView.DismissActionProvider getDismissActionProvider() {
            return new MessageCardView.DismissActionProvider() {
                @Override
                public void dismiss(int messageType) {
                    TabSuggestionMessageService.this.dismiss(
                            mTabSuggestion, mTabSuggestionFeedback);
                }
            };
        }

        /**
         * @return The class associated with handling the multi favicon icon provider for the large
         *     message card view. This includes building the background and fetching 3 favicons from
         *     the suggested tab list.
         */
        public MultiFaviconIconProvider createMultiFaviconIconProvider(Context context) {
            return new MultiFaviconIconProvider(context, mTabSuggestion, mProfile);
        }
    }

    private final Context mContext;
    private final Profile mProfile;
    private final Supplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final Supplier<TabListEditorCoordinator.TabListEditorController>
            mTabListEditorControllerSupplier;
    private final CustomMessageCardProvider mCustomMessageCardProvider;
    private final View mCustomCardView;
    private final PropertyModel mModel;

    public TabSuggestionMessageService(
            Context context,
            Profile profile,
            Supplier<TabModelFilter> currentTabModelFilterSupplier,
            Supplier<TabListEditorCoordinator.TabListEditorController>
                    tabListEditorControllerSupplier) {
        super(MessageType.TAB_SUGGESTION);
        mContext = context;
        mProfile = profile;
        mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
        mTabListEditorControllerSupplier = tabListEditorControllerSupplier;
        mCustomMessageCardProvider = this;
        mCustomCardView =
                LayoutInflater.from(context).inflate(R.layout.declutter_message_card_layout, null);
        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(DECLUTTER_INFO_TEXT, R.plurals.tab_declutter_message_card_text_info)
                        .with(
                                ARCHIVED_TAB_COUNT,
                                currentTabModelFilterSupplier.get().getTotalTabCount())
                        .with(ARCHIVED_TABS_EXPAND_CLICK_HANDLER, () -> {})
                        .with(DECLUTTER_SETTINGS_CLICK_HANDLER, () -> {})
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mCustomCardView, DeclutterMessageCardViewBinder::bind);
    }

    @VisibleForTesting
    void review(
            @NonNull TabSuggestion tabSuggestion,
            @NonNull Callback<TabSuggestionFeedback> feedbackCallback) {
        TabListEditorCoordinator.TabListEditorController tabListEditorController =
                mTabListEditorControllerSupplier.get();
        assert tabListEditorController != null;

        tabListEditorController.configureToolbarWithMenuItems(
                Collections.singletonList(getAction(tabSuggestion, feedbackCallback)),
                getNavigationProvider(tabSuggestion, feedbackCallback));

        tabListEditorController.show(
                getTabListFromSuggestion(tabSuggestion),
                tabSuggestion.getTabsInfo().size(),
                /* recyclerViewPosition= */ null);
    }

    @VisibleForTesting
    TabListEditorAction getAction(
            TabSuggestion tabSuggestion, Callback<TabSuggestionFeedback> feedbackCallback) {
        TabListEditorAction action;
        switch (tabSuggestion.getAction()) {
            case TabSuggestion.TabSuggestionAction.CLOSE:
                action =
                        TabListEditorCloseAction.createAction(
                                mContext,
                                TabListEditorAction.ShowMode.IF_ROOM,
                                TabListEditorAction.ButtonType.TEXT,
                                TabListEditorAction.IconPosition.END);
                break;
            default:
                assert false;
                return null;
        }

        action.addActionObserver(
                new TabListEditorAction.ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> selectedTabs) {
                        int totalTabCountBeforeProcess =
                                mCurrentTabModelFilterSupplier.get().getTabModel().getCount();
                        List<Integer> selectedTabIds = new ArrayList<>();
                        for (int i = 0; i < selectedTabs.size(); i++) {
                            selectedTabIds.add(selectedTabs.get(i).getId());
                        }
                        accept(
                                selectedTabIds,
                                totalTabCountBeforeProcess,
                                tabSuggestion,
                                feedbackCallback);
                    }
                });
        return action;
    }

    @VisibleForTesting
    TabListEditorCoordinator.TabListEditorNavigationProvider getNavigationProvider(
            TabSuggestion tabSuggestion,
            @NonNull Callback<TabSuggestionFeedback> feedbackCallback) {
        return new TabListEditorCoordinator.TabListEditorNavigationProvider(
                mContext, mTabListEditorControllerSupplier.get()) {
            @Override
            public void goBack() {
                super.goBack();

                feedbackCallback.onResult(
                        new TabSuggestionFeedback(tabSuggestion, DISMISSED, null, 0));
            }
        };
    }

    private List<Tab> getTabListFromSuggestion(TabSuggestion tabSuggestion) {
        List<Tab> tabs = new ArrayList<>();

        List<TabContext.TabInfo> suggestedTabInfo = tabSuggestion.getTabsInfo();
        TabModel model = mCurrentTabModelFilterSupplier.get().getTabModel();
        for (int i = 0; i < suggestedTabInfo.size(); i++) {
            int tabId = suggestedTabInfo.get(i).id;
            Tab tab = TabModelUtils.getTabById(model, tabId);
            if (tab == null) continue;

            tabs.add(tab);
        }
        return tabs;
    }

    @VisibleForTesting
    public void dismiss(
            @NonNull TabSuggestion tabSuggestion,
            @NonNull Callback<TabSuggestionFeedback> feedbackCallback) {
        feedbackCallback.onResult(
                new TabSuggestionFeedback(tabSuggestion, NOT_CONSIDERED, null, 0));
    }

    private void accept(
            List<Integer> selectedTabIds,
            int totalTabCount,
            @NonNull TabSuggestion tabSuggestion,
            @NonNull Callback<TabSuggestionFeedback> feedbackCallback) {
        feedbackCallback.onResult(
                new TabSuggestionFeedback(tabSuggestion, ACCEPTED, selectedTabIds, totalTabCount));
    }

    // TabSuggestionObserver implementations.
    @Override
    public void onNewSuggestion(
            List<TabSuggestion> tabSuggestions,
            Callback<TabSuggestionFeedback> tabSuggestionFeedback) {
        if (tabSuggestions.size() == 0) return;

        assert tabSuggestionFeedback != null;

        sSuggestionAvailableForTesting = true;
        for (TabSuggestion tabSuggestion : tabSuggestions) {
            sendAvailabilityNotification(
                    new TabSuggestionMessageData(
                            tabSuggestion,
                            tabSuggestionFeedback,
                            mCustomMessageCardProvider));
        }
    }

    @Override
    public void onTabSuggestionInvalidated() {
        sSuggestionAvailableForTesting = false;
        sendInvalidNotification();
    }

    public static boolean isSuggestionAvailableForTesting() {
        return sSuggestionAvailableForTesting;
    }

    // CustomMessageCardProvider implementation
    @Override
    public View getCustomView() {
        return mCustomCardView;
    }

    @Override
    public int getMessageCardVisibilityControl() {
        return MessageCardViewProperties.MessageCardScope.REGULAR;
    }

    @Override
    public int getCardType() {
        return TabListModel.CardProperties.ModelType.MESSAGE;
    }

    @Override
    public void setIsIncognito(boolean isIncognito) {
        // Intentional noop - this card will not appear on incognito.
    }
}
