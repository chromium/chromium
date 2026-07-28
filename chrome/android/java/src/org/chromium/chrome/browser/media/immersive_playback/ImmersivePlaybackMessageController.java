// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.content_public.browser.ImmersivePlaybackConfirmationStatus;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * Controller for the immersive playback confirmation message banner. It handles the message
 * interactions and shows the format selection dialog if the user clicks "Yes".
 */
@NullMarked
public class ImmersivePlaybackMessageController {
    private final Context mContext;
    private final Supplier<@Nullable MessageDispatcher> mMessageDispatcherSupplier;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final Tab mTab;
    private final @Nullable FullscreenManager mFullscreenManager;
    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    dismiss();
                }

                @Override
                public void onContentChanged(Tab tab) {
                    dismiss();
                }
            };
    private final FullscreenManager.Observer mFullscreenObserver =
            new FullscreenManager.Observer() {
                @Override
                public void onExitFullscreen(Tab tab) {
                    dismiss();
                }
            };
    private @Nullable ImmersivePlaybackConfirmationCallback mCallback;
    private @Nullable ImmersiveVideoFormatSelectionDialog mDialog;
    private @Nullable PropertyModel mMessageModel;
    private boolean mAreObserversRegistered;
    private int mRecommendedStereoMode = ImmersiveStereoMode.MONO;
    private int mRecommendedProjectionType = ImmersiveProjectionType.QUAD;

    public ImmersivePlaybackMessageController(
            Context context,
            Supplier<@Nullable MessageDispatcher> messageDispatcherSupplier,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            Tab tab,
            @Nullable FullscreenManager fullscreenManager) {
        mContext = context;
        mMessageDispatcherSupplier = messageDispatcherSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mTab = tab;
        mFullscreenManager = fullscreenManager;
    }

    /**
     * Shows the message banner.
     *
     * @param callback Callback to be invoked when the message is dismissed.
     * @param recommendedStereoMode The recommended stereo mode for the video.
     * @param recommendedProjectionType The recommended projection type for the video.
     */
    public void show(
            ImmersivePlaybackConfirmationCallback callback,
            @ImmersiveStereoMode int recommendedStereoMode,
            @ImmersiveProjectionType int recommendedProjectionType) {
        dismiss();
        mCallback = callback;
        mRecommendedStereoMode = recommendedStereoMode;
        mRecommendedProjectionType = recommendedProjectionType;

        if (mModalDialogManagerSupplier.get() == null) {
            reportResultAndReset(
                    ImmersivePlaybackConfirmationStatus.FAILED,
                    ImmersiveStereoMode.MONO,
                    ImmersiveProjectionType.QUAD);
            return;
        }

        registerObservers();
        showMessage();
    }

    /** Dismisses the message and dialog if showing. */
    public void dismiss() {
        unregisterObservers();
        if (mMessageModel != null) {
            PropertyModel model = mMessageModel;
            mMessageModel = null;
            MessageDispatcher messageDispatcher = mMessageDispatcherSupplier.get();
            if (messageDispatcher != null) {
                messageDispatcher.dismissMessage(model, DismissReason.DISMISSED_BY_FEATURE);
            }
        }
        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
        if (mCallback != null) {
            reportResultAndReset(
                    ImmersivePlaybackConfirmationStatus.CANCELED,
                    ImmersiveStereoMode.MONO,
                    ImmersiveProjectionType.QUAD);
        }
    }

    private @PrimaryActionClickBehavior int handlePrimaryAction() {
        unregisterObservers();

        if (mRecommendedStereoMode != ImmersiveStereoMode.MONO
                || mRecommendedProjectionType != ImmersiveProjectionType.QUAD) {
            reportResultAndReset(
                    ImmersivePlaybackConfirmationStatus.CONFIRMED,
                    mRecommendedStereoMode,
                    mRecommendedProjectionType);
            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
        }

        ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
        if (modalDialogManager == null) {
            reportResultAndReset(
                    ImmersivePlaybackConfirmationStatus.FAILED,
                    ImmersiveStereoMode.MONO,
                    ImmersiveProjectionType.QUAD);
            return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
        }

        showDialog(modalDialogManager);
        return PrimaryActionClickBehavior.DISMISS_IMMEDIATELY;
    }

    private void handleDismissed(@DismissReason int dismissReason) {
        mMessageModel = null;
        unregisterObservers();
        if (dismissReason != DismissReason.PRIMARY_ACTION) {
            reportResultAndReset(
                    ImmersivePlaybackConfirmationStatus.DECLINED,
                    ImmersiveStereoMode.MONO,
                    ImmersiveProjectionType.QUAD);
        }
    }

    private void showMessage() {
        MessageDispatcher messageDispatcher = mMessageDispatcherSupplier.get();
        WebContents webContents = mTab.getWebContents();
        if (messageDispatcher == null || webContents == null) {
            reportResultAndReset(
                    ImmersivePlaybackConfirmationStatus.FAILED,
                    ImmersiveStereoMode.MONO,
                    ImmersiveProjectionType.QUAD);
            return;
        }

        String title = mContext.getString(R.string.immersive_playback_confirmation_message);
        String buttonText = mContext.getString(R.string.immersive_playback_confirmation_yes);

        mMessageModel =
                new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                        .with(
                                MessageBannerProperties.MESSAGE_IDENTIFIER,
                                MessageIdentifier.IMMERSIVE_PLAYBACK_CONFIRMATION)
                        .with(MessageBannerProperties.TITLE, title)
                        .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, buttonText)
                        .with(MessageBannerProperties.ON_PRIMARY_ACTION, this::handlePrimaryAction)
                        .with(MessageBannerProperties.ON_DISMISSED, this::handleDismissed)
                        .build();

        messageDispatcher.enqueueMessage(
                mMessageModel, webContents, MessageScopeType.NAVIGATION, false);
    }

    private void showDialog(ModalDialogManager modalDialogManager) {
        mDialog =
                new ImmersiveVideoFormatSelectionDialog(
                        mContext, modalDialogManager, this::reportResultAndReset);
        mDialog.show();
    }

    private void registerObservers() {
        if (mAreObserversRegistered) return;
        mAreObserversRegistered = true;

        mTab.addObserver(mTabObserver);
        if (mFullscreenManager != null) {
            mFullscreenManager.addObserver(mFullscreenObserver);
        }
    }

    private void unregisterObservers() {
        if (!mAreObserversRegistered) return;
        mAreObserversRegistered = false;

        mTab.removeObserver(mTabObserver);
        if (mFullscreenManager != null) {
            mFullscreenManager.removeObserver(mFullscreenObserver);
        }
    }

    private void reportResultAndReset(
            @ImmersivePlaybackConfirmationStatus int status,
            @ImmersiveStereoMode int stereoMode,
            @ImmersiveProjectionType int projectionType) {
        mDialog = null;
        if (mCallback != null) {
            mCallback.onResult(status, stereoMode, projectionType);
            mCallback = null;
        }
    }
}
