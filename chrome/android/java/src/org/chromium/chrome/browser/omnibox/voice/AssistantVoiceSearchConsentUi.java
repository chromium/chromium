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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantPreferenceFragment;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
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
    private static final String CONSENT_OUTCOME_HISTOGRAM = "Assistant.VoiceSearch.ConsentOutcome";

    /**
     * Show the consent ui to the user.
     * @param windowAndroid The current {@link WindowAndroid} for the app.
     * @param sharedPreferencesManager The {@link SharedPreferencesManager} to read/write prefs.
     * @param settingsLauncher The {@link SettingsLauncher}, used to launch settings.
     * @param completionCallback A callback to be invoked if the user is continuing with the
     *                           requested voice search.
     */
    static void show(WindowAndroid windowAndroid, SharedPreferencesManager sharedPreferencesManager,
            SettingsLauncher settingsLauncher, Callback<Boolean> completionCallback) {
        // TODO(wylieb): Inject BottomSheetController into this class properly.
        AssistantVoiceSearchConsentUi consentUi = new AssistantVoiceSearchConsentUi(windowAndroid,
                windowAndroid.getContext().get(), sharedPreferencesManager, settingsLauncher,
                BottomSheetControllerProvider.from(windowAndroid));
        consentUi.show(completionCallback);
    }

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
    private final SettingsLauncher mSettingsLauncher;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private View mContentView;

    private @Nullable Callback<Boolean> mCompletionCallback;

    @VisibleForTesting
    AssistantVoiceSearchConsentUi(WindowAndroid windowAndroid, Context context,
            SharedPreferencesManager sharedPreferencesManager, SettingsLauncher settingsLauncher,
            BottomSheetController bottomSheetController) {
        mContext = context;
        mSharedPreferencesManager = sharedPreferencesManager;
        mSettingsLauncher = settingsLauncher;
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
                    onConsentRejected();
                    RecordHistogram.recordEnumeratedHistogram(CONSENT_OUTCOME_HISTOGRAM,
                            ConsentOutcome.REJECTED_VIA_DISMISS, ConsentOutcome.MAX_VALUE);
                }
                mCompletionCallback.onResult(mSharedPreferencesManager.readBoolean(
                        ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false));
            }
        };

        View acceptButton = mContentView.findViewById(R.id.button_primary);
        acceptButton.setOnClickListener((v) -> {
            onConsentAccepted();
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        });

        View cancelButton = mContentView.findViewById(R.id.button_secondary);
        cancelButton.setOnClickListener((v) -> {
            onConsentRejected();
            mBottomSheetController.hideContent(this, /* animate= */ true,
                    BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        });

        View learnMore = mContentView.findViewById(R.id.avs_consent_ui_learn_more);
        learnMore.setOnClickListener((v) -> openLearnMore());
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

    private void onConsentAccepted() {
        mSharedPreferencesManager.writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, true);
        RecordHistogram.recordEnumeratedHistogram(CONSENT_OUTCOME_HISTOGRAM,
                ConsentOutcome.ACCEPTED_VIA_BUTTON, ConsentOutcome.MAX_VALUE);
    }

    private void onConsentRejected() {
        mSharedPreferencesManager.writeBoolean(ASSISTANT_VOICE_SEARCH_ENABLED, false);
        RecordHistogram.recordEnumeratedHistogram(CONSENT_OUTCOME_HISTOGRAM,
                ConsentOutcome.REJECTED_VIA_BUTTON, ConsentOutcome.MAX_VALUE);
    }

    /** Open a page to learn more about the consent dialog. */
    private void openLearnMore() {
        mSettingsLauncher.launchSettingsActivity(
                mContext, AutofillAssistantPreferenceFragment.class, /* fragmentArgs= */ null);
    }

    // WindowAndroid.ActivityStateObserver implementation.

    @Override
    public void onActivityResumed() {
        // It's possible the user clicked through "learn more" and enabled/disabled it via settings.
        if (!mSharedPreferencesManager.contains(ASSISTANT_VOICE_SEARCH_ENABLED)) return;
        RecordHistogram.recordEnumeratedHistogram(CONSENT_OUTCOME_HISTOGRAM,
                mSharedPreferencesManager.readBoolean(
                        ASSISTANT_VOICE_SEARCH_ENABLED, /* default= */ false)
                        ? ConsentOutcome.ACCEPTED_VIA_SETTINGS
                        : ConsentOutcome.REJECTED_VIA_SETTINGS,
                ConsentOutcome.MAX_VALUE);
        mBottomSheetController.hideContent(this, /* animate= */ true,
                BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
    }

    @Override
    public void onActivityPaused() {}

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