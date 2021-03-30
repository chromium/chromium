// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.ASSISTANT_VOICE_SEARCH_ENABLED;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The consent ui shown to users when Chrome attempts to use Assistant voice search for the
 * first time.
 */
class AssistantVoiceSearchConsentUi
        implements BottomSheetContent, WindowAndroid.ActivityStateObserver {
    @VisibleForTesting
    static final String CONSENT_OUTCOME_HISTOGRAM = "Assistant.VoiceSearch.ConsentOutcome";

    /**
     * Show the consent ui to the user.
     * @param windowAndroid The current {@link WindowAndroid} for the app.
     * @param sharedPreferencesManager The {@link SharedPreferencesManager} to read/write prefs.
     * @param launchAssistanceSettingsAction Runnable launching settings activity.
     * @param bottomSheetController The {@link BottomSheetController} used to show the consent ui.
     *                              This can be null when starting the consent flow from
     *                              SearchActivity.
     * @param completionCallback A callback to be invoked if the user is continuing with the
     *                           requested voice search.
     */
    static void show(@NonNull WindowAndroid windowAndroid,
            @NonNull SharedPreferencesManager sharedPreferencesManager,
            @NonNull Runnable launchAssistanceSettingsAction,
            @Nullable BottomSheetController bottomSheetController,
            @NonNull Callback<Boolean> completionCallback) {
        // When attempting voice search through the search widget, the bottom sheet isn't
        // available. When this happens, bail out of the consent flow and fallback to system-ui.
        // Consent will be retried the next time.

        if (bottomSheetController == null) {
            PostTask.postTask(TaskTraits.USER_VISIBLE,
                    () -> { completionCallback.onResult(/* useAssistant= */ false); });
            return;
        }
        AssistantVoiceSearchConsentUi consentUi = new AssistantVoiceSearchConsentUi(windowAndroid,
                windowAndroid.getContext().get(), sharedPreferencesManager,
                launchAssistanceSettingsAction, bottomSheetController);
        consentUi.show(completionCallback);
    }

    // AssistantConsentOutcome defined in tools/metrics/histograms/enums.xml. Do not reorder or
    // remove items, only add new items before HISTOGRAM_BOUNDARY.
    @IntDef({ConsentOutcome.ACCEPTED_VIA_BUTTON, ConsentOutcome.ACCEPTED_VIA_SETTINGS,
            ConsentOutcome.REJECTED_VIA_BUTTON, ConsentOutcome.REJECTED_VIA_SETTINGS,
            ConsentOutcome.REJECTED_VIA_DISMISS, ConsentOutcome.MAX_VALUE})
    @Retention(RetentionPolicy.SOURCE)
    @interface ConsentOutcome {
        int ACCEPTED_VIA_BUTTON = 0;
        int ACCEPTED_VIA_SETTINGS = 1;
        int REJECTED_VIA_BUTTON = 2;
        int REJECTED_VIA_SETTINGS = 3;
        int REJECTED_VIA_DISMISS = 4;

        // STOP: When updating this, also update values in enums.xml.
        int MAX_VALUE = 5;
    }

    private final WindowAndroid mWindowAndroid;
    private final Context mContext;
    private final SharedPreferencesManager mSharedPreferencesManager;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private View mContentView;

    private @Nullable Callback<Boolean> mCompletionCallback;

    @VisibleForTesting
    AssistantVoiceSearchConsentUi(@NonNull WindowAndroid windowAndroid, @NonNull Context context,
            @NonNull SharedPreferencesManager sharedPreferencesManager,
            @NonNull Runnable launchAssistanceSettingsAction,
            @NonNull BottomSheetController bottomSheetController) {
        mContext = context;
        mSharedPreferencesManager = sharedPreferencesManager;
        mBottomSheetController = bottomSheetController;
        mWindowAndroid = windowAndroid;
        mWindowAndroid.addActivityStateObserver(this);

        mContentView = LayoutInflater.from(context).inflate(
                R.layout.assistant_voice_search_consent_ui, /* root= */ null);

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                if (reason == BottomSheetController.StateChangeReason.TAP_SCRIM
                        || reason == BottomSheetController.StateChangeReason.BACK_PRESS) {
                    // The user dismissed the dialog without pressing a button.
                    onConsentRejected(ConsentOutcome.REJECTED_VIA_DISMISS);
                }
                mCompletionCallback.onResult(mSharedPreferencesManager.readBoolean(
                        ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false));
            }
        };

        View acceptButton = mContentView.findViewById(R.id.button_primary);
        acceptButton.setOnClickListener((v) -> {
            onConsentAccepted(ConsentOutcome.ACCEPTED_VIA_BUTTON);
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        });

        View cancelButton = mContentView.findViewById(R.id.button_secondary);
        cancelButton.setOnClickListener((v) -> {
            onConsentRejected(ConsentOutcome.REJECTED_VIA_BUTTON);
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        });

        View learnMore = mContentView.findViewById(R.id.avs_consent_ui_learn_more);
        learnMore.setOnClickListener((v) -> launchAssistanceSettingsAction.run());
    }

    /**
     * Show the dialog with the given callback.
     * @param completionCallback Callback to be invoked if the user continues with the requested
     *                           voice search.
     */
    @VisibleForTesting
    void show(@NonNull Callback<Boolean> completionCallback) {
        assert mCompletionCallback == null;
        assert !mBottomSheetController.isSheetOpen();
        mCompletionCallback = completionCallback;

        if (!mBottomSheetController.requestShowContent(this, /* animate= */ true)) {
            mBottomSheetController.hideContent(
                    this, /* animate= */ false, BottomSheetController.StateChangeReason.NONE);
            completionCallback.onResult(mSharedPreferencesManager.readBoolean(
                    ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false));
            destroy();
        } else {
            mBottomSheetController.addObserver(mBottomSheetObserver);
        }
    }

    private void onConsentAccepted(@ConsentOutcome int consentOutcome) {
        mSharedPreferencesManager.writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, true);
        RecordHistogram.recordEnumeratedHistogram(
                CONSENT_OUTCOME_HISTOGRAM, consentOutcome, ConsentOutcome.MAX_VALUE);
    }

    private void onConsentRejected(@ConsentOutcome int consentOutcome) {
        mSharedPreferencesManager.writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, false);
        RecordHistogram.recordEnumeratedHistogram(
                CONSENT_OUTCOME_HISTOGRAM, consentOutcome, ConsentOutcome.MAX_VALUE);
    }

    // WindowAndroid.ActivityStateObserver implementation.

    @Override
    public void onActivityResumed() {
        // It's possible the user clicked through "learn more" and enabled/disabled it via settings.
        if (!mSharedPreferencesManager.contains(ASSISTANT_VOICE_SEARCH_ENABLED)) return;
        if (mSharedPreferencesManager.readBoolean(
                    ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false)) {
            onConsentAccepted(ConsentOutcome.ACCEPTED_VIA_SETTINGS);
        } else {
            onConsentRejected(ConsentOutcome.REJECTED_VIA_SETTINGS);
        }
        mBottomSheetController.hideContent(this, /* animate= */ true,
                BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
    }

    @Override
    public void onActivityPaused() {}

    @Override
    public void onActivityDestroyed() {}

    // BottomSheetContent implementation.

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public @Nullable View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        mCompletionCallback = null;
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mWindowAndroid.removeActivityStateObserver(AssistantVoiceSearchConsentUi.this);
    }

    @Override
    public @ContentPriority int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getPeekHeight() {
        // Disable the peeking behavior for this dialog.
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.avs_consent_ui_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.avs_consent_ui_half_height_description;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.avs_consent_ui_full_height_description;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.avs_consent_ui_closed_description;
    }
}
