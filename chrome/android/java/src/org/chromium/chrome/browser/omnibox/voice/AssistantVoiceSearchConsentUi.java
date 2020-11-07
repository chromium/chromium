// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * The consent ui shown to users when Chrome attempts to use Assistant voice search for the
 * first time.
 */
class AssistantVoiceSearchConsentUi implements BottomSheetContent {
    /**
     * Show the consent ui to the user.
     * @param windowAndroid The current {@link WindowAndroid} for the app.
     * @param completionCallback A Runnable to be invoked if the user is continuing with the
     *                           requested voice search
     */
    static void show(WindowAndroid windowAndroid, Runnable completionCallback) {
        // TODO(wylieb): Inject BottomSheetController into this class properly.
        AssistantVoiceSearchConsentUi consentUi =
                new AssistantVoiceSearchConsentUi(windowAndroid.getContext().get(),
                        BottomSheetControllerProvider.from(windowAndroid));
        consentUi.show(completionCallback);
    }

    private BottomSheetController mBottomSheetController;
    private BottomSheetObserver mBottomSheetObserver;
    private View mContentView;

    private @Nullable Runnable mCompletionCallback;

    private AssistantVoiceSearchConsentUi(
            Context context, BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;

        mContentView = LayoutInflater.from(context).inflate(
                R.layout.assistant_voice_search_consent_ui, /* root= */ null);

        mBottomSheetObserver = new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@BottomSheetController.StateChangeReason int reason) {
                if (reason == BottomSheetController.StateChangeReason.TAP_SCRIM
                        || reason == BottomSheetController.StateChangeReason.BACK_PRESS) {
                    // The user dismissed the dialog without pressing a button.
                    onConsentRejected();
                    // TODO(wylieb): Record metrics here.
                }
                mCompletionCallback.run();
                mCompletionCallback = null;
                mBottomSheetController.removeObserver(mBottomSheetObserver);
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
    private void show(@NonNull Runnable completionCallback) {
        assert mCompletionCallback == null;
        assert !mBottomSheetController.isSheetOpen();
        mCompletionCallback = completionCallback;

        mBottomSheetController.requestShowContent(this, /* animate= */ true);
        mBottomSheetController.addObserver(mBottomSheetObserver);
        // TODO(wylieb): Record metrics here.
    }

    private void onConsentAccepted() {
        // TODO(wylieb): Implement this.
        // TODO(wylieb): Record metrics here.
    }

    private void onConsentRejected() {
        // TODO(wylieb): Implement this.
        // TODO(wylieb): Record metrics here.
    }

    /** Open a page to learn more about the consent dialog. */
    private void openLearnMore() {
        // TODO(wylieb): Implement this.
        // TODO(wylieb): Record metrics here.
    }

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
    public void destroy() {}

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