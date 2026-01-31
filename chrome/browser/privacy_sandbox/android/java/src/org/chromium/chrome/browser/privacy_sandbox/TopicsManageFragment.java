// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.HashSet;
import java.util.List;
import java.util.function.Supplier;

/** Fragment for managing all the topics. */
@NullMarked
public class TopicsManageFragment extends PrivacySandboxSettingsBaseFragment {
    private static final String MANAGE_TOPICS_PREFERENCE = "topics_list";
    private static final String TOPICS_PREF_PREFIX = "topic_";

    private PreferenceCategory mTopicsCategory;

    private @Nullable Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        mPageTitle.set(getString(R.string.settings_topics_page_manage_topics_heading));
        SettingsUtils.addPreferencesFromResource(this, R.xml.topics_manage_preference);

        mTopicsCategory = findPreference(MANAGE_TOPICS_PREFERENCE);
        mTopicsCategory.setSummary(
                SpanApplier.applySpans(
                        getResources()
                                .getString(R.string.settings_topics_page_manage_topics_explanation),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(getContext(), this::onLearnMoreClicked))));

        populateTopics();
        RecordUserAction.record("Settings.PrivacySandbox.Topics.Manage.PageOpened");
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    /**
     * Sets Supplier for {@lnk ModalDialogManager} used to display {@link
     * AutofillDeleteCreditCardConfirmationDialog}.
     */
    public void setModalDialogManagerSupplier(
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier) {
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    private void populateTopics() {
        mTopicsCategory.removeAll();
        List<Topic> firstLevelTopics = getPrivacySandboxBridge().getFirstLevelTopics();
        var blockedTopics = new HashSet<Topic>(getPrivacySandboxBridge().getBlockedTopics());
        for (Topic topic : firstLevelTopics) {
            var preference = new TopicSwitchPreference(getContext(), topic);
            // LINT.IfChange(TopicsKey)
            String key = TOPICS_PREF_PREFIX + topic.getTopicId();
            // LINT.ThenChange(:DynamicTopicsKey)
            preference.setKey(key);
            preference.setChecked(!blockedTopics.contains(topic));
            preference.setOnPreferenceChangeListener(this::onToggleChange);
            mTopicsCategory.addPreference(preference);
        }
    }

    private boolean onToggleChange(Preference preference, Object newValue) {
        var topicPreference = (TopicSwitchPreference) preference;
        if (!((boolean) newValue)) {
            return handleBlockTopic(topicPreference);
        }
        getPrivacySandboxBridge().setTopicAllowed(topicPreference.getTopic(), true);
        RecordUserAction.record("Settings.PrivacySandbox.Topics.Manage.TopicEnabled");
        return true;
    }

    private boolean handleBlockTopic(TopicSwitchPreference preference) {
        Topic topic = preference.getTopic();
        // Check if a child level topic is assigned.
        List<Topic> childTopics = getPrivacySandboxBridge().getChildTopicsCurrentlyAssigned(topic);
        if (childTopics.isEmpty()) {
            getPrivacySandboxBridge().setTopicAllowed(topic, false);
            RecordUserAction.record("Settings.PrivacySandbox.Topics.Manage.TopicBlocked");
            return true;
        }
        // There are assigned child topics - display a confirmation prompt.
        assert mModalDialogManagerSupplier != null;
        ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
        assert modalDialogManager != null;
        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(
                        modalDialogManager,
                        dismissalCause -> {
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                getPrivacySandboxBridge().setTopicAllowed(topic, false);
                                RecordUserAction.record(
                                        "Settings.PrivacySandbox.Topics.Manage.TopicBlockingConfirmed");
                            } else {
                                preference.setChecked(true);
                                RecordUserAction.record(
                                        "Settings.PrivacySandbox.Topics.Manage.TopicBlockingCanceled");
                            }
                        });
        PropertyModel dialog =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(
                                ModalDialogProperties.TITLE,
                                getContext()
                                        .getString(
                                                R.string.settings_manage_topics_dialog_clank_title,
                                                topic.getName()))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                getContext()
                                        .getString(
                                                R.string.settings_manage_topics_dialog_clank_body,
                                                topic.getName()))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                getContext().getString(R.string.settings_topics_page_block_topic))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                getContext().getString(R.string.cancel))
                        .build();
        modalDialogManager.showDialog(dialog, ModalDialogManager.ModalDialogType.APP);
        return true;
    }

    private void onLearnMoreClicked(View view) {
        RecordUserAction.record("Settings.PrivacySandbox.Topics.Manage.LearnMoreClicked");
        getCustomTabLauncher()
                .openUrlInCct(getContext(), PrivacySandboxSettingsFragment.HELP_CENTER_URL);
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    TopicsManageFragment.class.getName(), R.xml.topics_manage_preference) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    PrivacySandboxBridge bridge = new PrivacySandboxBridge(profile);

                    List<Topic> topics = bridge.getFirstLevelTopics();

                    for (Topic topic : topics) {
                        String title = topic.getName();
                        String summary = topic.getDescription();

                        // LINT.IfChange(DynamicTopicsKey)
                        String key = TOPICS_PREF_PREFIX + topic.getTopicId();
                        // LINT.ThenChange(:TopicsKey)

                        String uniqueId = getUniqueId(key);

                        SettingsIndexData.Entry entry =
                                new SettingsIndexData.Entry.Builder(
                                                uniqueId,
                                                key,
                                                title,
                                                TopicsManageFragment.class.getName())
                                        .setSummary(summary)
                                        .build();

                        indexData.updateEntry(uniqueId, entry);
                    }
                }
            };
}
