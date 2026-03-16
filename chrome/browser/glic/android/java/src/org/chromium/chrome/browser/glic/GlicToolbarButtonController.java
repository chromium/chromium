// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.content.Context;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/** Defines a toolbar button to open the Glic bottom sheet. */
@NullMarked
public class GlicToolbarButtonController extends BaseButtonDataProvider
        implements ActorKeyedService.Observer {
    @IntDef({ButtonState.DEFAULT, ButtonState.WORKING, ButtonState.NEEDS_REVIEW})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ButtonState {
        int DEFAULT = 0;
        int WORKING = 1;
        int NEEDS_REVIEW = 2;
    }

    private final Runnable mToggleGlicCallback;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;
    private @Nullable Profile mCurrentProfile;
    private @Nullable ActorKeyedService mCurrentActorService;
    private final ButtonSpec mDefaultSpec;
    private final ButtonSpec mReviewSpec;
    private final ButtonSpec mWorkingSpec;

    private @ButtonState int mButtonState = ButtonState.DEFAULT;

    /**
     * @param context The Android context.
     * @param activeTabSupplier The currently active tab.
     * @param toggleGlicCallback Callback to run when the button is clicked to open Glic.
     * @param trackerSupplier Supplier for the current profile tracker.
     */
    public GlicToolbarButtonController(
            Context context,
            Supplier<@Nullable Tab> activeTabSupplier,
            Runnable toggleGlicCallback,
            Supplier<@Nullable Tracker> trackerSupplier) {
        // TODO(crbug.com/482372270): Add correct styling to button including Nudge state text,
        // active state shape change, and appropriate colors.
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                AppCompatResources.getDrawable(context, R.drawable.ic_spark_24dp),
                context.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                /* actionChipLabelResId= */ Resources.ID_NULL,
                /* supportsTinting= */ true,
                /* iphCommandBuilder= */ null,
                AdaptiveToolbarButtonVariant.GLIC,
                /* tooltipTextResId= */ Resources.ID_NULL);
        mToggleGlicCallback = toggleGlicCallback;
        mTrackerSupplier = trackerSupplier;
        mDefaultSpec = mButtonData.getButtonSpec();
        mReviewSpec =
                new ButtonSpec(
                        mDefaultSpec.getDrawable(),
                        this,
                        mDefaultSpec.getOnLongClickListener(),
                        mDefaultSpec.getContentDescription(),
                        mDefaultSpec.getSupportsTinting(),
                        mDefaultSpec.getIphCommandBuilder(),
                        mDefaultSpec.getButtonVariant(),
                        R.string.glic_button_status_review,
                        mDefaultSpec.getHoverTooltipTextId(),
                        mDefaultSpec.hasErrorBadge());

        // TODO(haileywang): Handle other button states.
        mWorkingSpec = mButtonData.getButtonSpec();
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        if (tab == null || tab.isOffTheRecord()) {
            mButtonData.setCanShow(false);
            return super.get(tab);
        }

        updateActorServiceObservation(tab.getProfile());
        updateButtonState();

        ButtonSpec desiredSpec = mDefaultSpec;
        switch (mButtonState) {
            case ButtonState.NEEDS_REVIEW:
                desiredSpec = mReviewSpec;
                break;
            case ButtonState.WORKING:
                desiredSpec = mWorkingSpec;
                break;
            case ButtonState.DEFAULT:
            default:
                desiredSpec = mDefaultSpec;
        }
        mButtonData.setButtonSpec(desiredSpec);

        // TODO(haileywang): We should double check whether Glic is enabled.
        mButtonData.setCanShow(true);
        mButtonData.setEnabled(true);
        return super.get(tab);
    }

    private void updateButtonState() {
        mButtonState = ButtonState.DEFAULT;

        if (mCurrentActorService != null) {
            ActorTask task = mCurrentActorService.getCurrentActiveTask();
            if (task != null) {
                @ActorTaskState int state = task.getState();
                switch (state) {
                    case ActorTaskState.WAITING_ON_USER:
                    case ActorTaskState.FINISHED:
                    case ActorTaskState.FAILED:
                        mButtonState = ButtonState.NEEDS_REVIEW;
                        break;
                    case ActorTaskState.ACTING:
                    case ActorTaskState.REFLECTING:
                        mButtonState = ButtonState.WORKING;
                        // TODO(haileywang): Start the animation of the working button.
                        break;
                    case ActorTaskState.PAUSED_BY_USER:
                    case ActorTaskState.PAUSED_BY_ACTOR:
                        mButtonState = ButtonState.WORKING;
                        break;
                    case ActorTaskState.CANCELLED:
                    case ActorTaskState.CREATED:
                        // Show the default button for these states.
                        break;
                    default:
                        throw new AssertionError("Unexpected task state: " + state);
                }
            }
        }
    }

    private void updateActorServiceObservation(Profile profile) {
        assert !profile.isOffTheRecord();
        if (profile.equals(mCurrentProfile)) return;

        if (mCurrentActorService != null) {
            mCurrentActorService.removeObserver(this);
        }

        mCurrentProfile = profile;
        mCurrentActorService = ActorKeyedServiceFactory.getForProfile(profile);

        if (mCurrentActorService != null) {
            mCurrentActorService.addObserver(this);
        }
    }

    @Override
    public void destroy() {
        if (mCurrentActorService != null) {
            mCurrentActorService.removeObserver(this);
            mCurrentActorService = null;
        }
        mCurrentProfile = null;
        super.destroy();
    }

    @Override
    public void onClick(View view) {
        mToggleGlicCallback.run();
        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_GLIC_CLICKED);
        }
    }

    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        int oldButtonState = mButtonState;
        updateButtonState();
        if (mButtonState != oldButtonState) {
            notifyObservers(true);
        }
    }
}
