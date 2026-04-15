// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.appcompat.content.res.AppCompatResources;

import com.airbnb.lottie.LottieCompositionFactory;
import com.airbnb.lottie.LottieDrawable;
import com.airbnb.lottie.LottieProperty;
import com.airbnb.lottie.model.KeyPath;
import com.airbnb.lottie.value.LottieValueCallback;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.actor.ActorTaskState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarFeatures;
import org.chromium.chrome.browser.toolbar.optional_button.BaseButtonDataProvider;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData.ButtonSpec;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/** Defines a toolbar button to open the Glic bottom sheet. */
@NullMarked
public class GlicToolbarButtonController extends BaseButtonDataProvider
        implements ActorKeyedService.Observer, GlicKeyedService.GlobalShowHideObserver {
    @IntDef({ButtonState.DEFAULT, ButtonState.WORKING, ButtonState.NEEDS_REVIEW, ButtonState.DONE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ButtonState {
        int DEFAULT = 0;
        int WORKING = 1;
        int NEEDS_REVIEW = 2;
        int DONE = 3;
    }

    /** Delegate interface for handling clicks on the Glic toolbar button. */
    @FunctionalInterface
    public interface GlicButtonDelegate {
        /**
         * Called when the Glic button is clicked.
         *
         * @param preventClose whether to prevent closing the Glic UI if it's already open.
         */
        void onClick(boolean preventClose);
    }

    private final Context mContext;
    private final GlicButtonDelegate mToggleGlicCallback;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;
    private final Supplier<ChromeAndroidTask> mTaskSupplier;
    private @Nullable Profile mCurrentProfile;
    private @Nullable ActorKeyedService mCurrentActorService;
    private @Nullable GlicKeyedService mCurrentGlicService;
    private final ButtonSpec mDefaultSpec;
    private final ButtonSpec mWorkingSpec;
    private final ButtonSpec mReviewSpec;
    private final ButtonSpec mDoneSpec;

    private @ButtonState int mButtonState = ButtonState.DEFAULT;
    private boolean mPersistDoneState;
    private boolean mIsPanelOpen;

    /**
     * @param context The Android context.
     * @param activeTabSupplier The currently active tab.
     * @param toggleGlicCallback Callback to run when the button is clicked to open Glic.
     * @param trackerSupplier Supplier for the current profile tracker.
     * @param taskSupplier Supplier for the ChromeAndroidTask.
     */
    public GlicToolbarButtonController(
            Context context,
            Supplier<@Nullable Tab> activeTabSupplier,
            GlicButtonDelegate toggleGlicCallback,
            Supplier<@Nullable Tracker> trackerSupplier,
            Supplier<ChromeAndroidTask> taskSupplier) {
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
        mContext = context;
        mToggleGlicCallback = toggleGlicCallback;
        mTrackerSupplier = trackerSupplier;
        mTaskSupplier = taskSupplier;
        mDefaultSpec = mButtonData.getButtonSpec();
        mWorkingSpec = createWorkingSpec(context);
        mReviewSpec = createReviewSpec();
        mDoneSpec = createDoneSpec();
    }

    private ButtonSpec createReviewSpec() {
        return new ButtonSpec.Builder(mDefaultSpec)
                .setActionChipLabelResId(R.string.glic_button_status_review)
                .setShouldSuppressCpa(true)
                .build();
    }

    private ButtonSpec createDoneSpec() {
        return new ButtonSpec.Builder(mDefaultSpec)
                .setActionChipLabelResId(R.string.glic_button_status_done)
                .setShouldSuppressCpa(true)
                .build();
    }

    private ButtonSpec createWorkingSpec(Context context) {
        // TODO(haileywang): Handle other button states.
        LottieDrawable lottieDrawable = new LottieDrawable();
        LottieCompositionFactory.fromRawRes(context, R.raw.glic_spinner)
                .addListener(
                        composition -> {
                            lottieDrawable.setComposition(composition);
                            lottieDrawable.setRepeatCount(LottieDrawable.INFINITE);
                            lottieDrawable.playAnimation();
                        });
        Drawable sparkIcon = AppCompatResources.getDrawable(context, R.drawable.ic_spark_24dp);
        LayerDrawable layerDrawable =
                new LayerDrawable(new Drawable[] {lottieDrawable, sparkIcon}) {
                    private @Nullable ColorStateList mTintList;

                    @Override
                    public void setTintList(@Nullable ColorStateList tint) {
                        super.setTintList(tint);
                        mTintList = tint;
                        updateLottieTint();
                    }

                    @Override
                    protected boolean onStateChange(int[] state) {
                        boolean changed = super.onStateChange(state);
                        if (updateLottieTint()) {
                            changed = true;
                        }
                        return changed;
                    }

                    @Override
                    public boolean isStateful() {
                        return (mTintList != null && mTintList.isStateful()) || super.isStateful();
                    }

                    private boolean updateLottieTint() {
                        if (mTintList != null) {
                            int color =
                                    mTintList.getColorForState(
                                            getState(), mTintList.getDefaultColor());
                            lottieDrawable.addValueCallback(
                                    new KeyPath("**"),
                                    LottieProperty.COLOR_FILTER,
                                    new LottieValueCallback<>(
                                            new PorterDuffColorFilter(
                                                    color, PorterDuff.Mode.SRC_IN)));
                            return true;
                        }
                        return false;
                    }
                };
        float density = context.getResources().getDisplayMetrics().density;
        // Adjust sizes of the spark and spinner.
        int sparkInset = Math.round(2 * density);
        int spinnerInset = Math.round(-10 * density);
        layerDrawable.setLayerInset(0, spinnerInset, spinnerInset, spinnerInset, spinnerInset);
        layerDrawable.setLayerInset(1, sparkInset, sparkInset, sparkInset, sparkInset);
        return new ButtonSpec.Builder(mDefaultSpec)
                .setDrawable(layerDrawable)
                .setShouldSuppressCpa(true)
                .build();
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        // TODO(crbug.com/499354469): Add proper checks for glic availability.
        if (!AdaptiveToolbarFeatures.isGlicActionEnabled()) {
            return false;
        }
        if (tab == null || tab.isOffTheRecord() || UrlUtilities.isNtpUrl(tab.getUrl())) {
            return false;
        }
        return super.shouldShowButton(tab);
    }

    @Override
    public ButtonData get(@Nullable Tab tab) {
        ButtonData buttonData = super.get(tab);
        if (!buttonData.canShow()) {
            return buttonData;
        }

        // This can be assumed because shouldShowButton hides the entrypoint if there's no tab.
        assumeNonNull(tab);
        updateObservations(tab.getProfile());
        updateButtonState();

        ButtonSpec desiredSpec = mDefaultSpec;
        switch (mButtonState) {
            case ButtonState.NEEDS_REVIEW:
                desiredSpec = mReviewSpec;
                break;
            case ButtonState.WORKING:
                desiredSpec = mWorkingSpec;
                break;
            case ButtonState.DONE:
                desiredSpec = mDoneSpec;
                break;
            case ButtonState.DEFAULT:
            default:
                desiredSpec = mDefaultSpec;
        }
        mButtonData.setButtonSpec(
                new ButtonSpec.Builder(desiredSpec).setIsChecked(mIsPanelOpen).build());

        mButtonData.setEnabled(true);
        return buttonData;
    }

    private void updateButtonState() {
        if (mCurrentActorService == null) {
            mButtonState = ButtonState.DEFAULT;
            return;
        }

        ActorTask task = mCurrentActorService.getCurrentActiveTask();
        if (task == null) {
            // Fallback to DONE state if it was persisted, otherwise DEFAULT.
            mButtonState = mPersistDoneState ? ButtonState.DONE : ButtonState.DEFAULT;
            return;
        }

        mPersistDoneState = false;
        @ActorTaskState int state = task.getState();
        switch (state) {
            case ActorTaskState.WAITING_ON_USER:
            case ActorTaskState.FAILED:
                mButtonState = ButtonState.NEEDS_REVIEW;
                break;
            case ActorTaskState.FINISHED:
                mButtonState = ButtonState.DONE;
                mPersistDoneState = true;
                break;
            case ActorTaskState.ACTING:
            case ActorTaskState.REFLECTING:
            case ActorTaskState.PAUSED_BY_USER:
            case ActorTaskState.PAUSED_BY_ACTOR:
                mButtonState = ButtonState.WORKING;
                break;
            case ActorTaskState.CANCELLED:
            case ActorTaskState.CREATED:
                mButtonState = ButtonState.DEFAULT;
                break;
            default:
                throw new AssertionError("Unexpected task state: " + state);
        }
    }

    private void updateIsPanelOpen() {
        if (mCurrentGlicService == null || mCurrentProfile == null) return;
        ChromeAndroidTask task = mTaskSupplier.get();
        if (task == null) return;

        long browserWindowPtr = task.getOrCreateNativeBrowserWindowPtr(mCurrentProfile);
        boolean isOpen = mCurrentGlicService.isPanelShowingForBrowser(browserWindowPtr);
        if (mIsPanelOpen != isOpen) {
            mIsPanelOpen = isOpen;
            notifyObservers(true);
        }
    }

    private void updateObservations(Profile profile) {
        assert !profile.isOffTheRecord();
        if (profile.equals(mCurrentProfile)) return;

        if (mCurrentActorService != null) {
            mCurrentActorService.removeObserver(this);
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.removeGlobalShowHideObserver(this);
        }

        mCurrentProfile = profile;
        mCurrentActorService = ActorKeyedServiceFactory.getForProfile(profile);
        mCurrentGlicService = GlicKeyedServiceFactory.getForProfile(profile);

        if (mCurrentActorService != null) {
            mCurrentActorService.addObserver(this);
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.addGlobalShowHideObserver(this);
            updateIsPanelOpen();
        }
    }

    @Override
    public void destroy() {
        if (mCurrentActorService != null) {
            mCurrentActorService.removeObserver(this);
            mCurrentActorService = null;
        }
        if (mCurrentGlicService != null) {
            mCurrentGlicService.removeGlobalShowHideObserver(this);
            mCurrentGlicService = null;
        }
        mCurrentProfile = null;
        super.destroy();
    }

    @Override
    protected @Nullable IphCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IphCommandBuilder(
                mContext.getResources(),
                FeatureConstants.GLIC_PROMO_ANDROID_FEATURE,
                R.string.iph_glic_promo_text,
                R.string.iph_glic_promo_accessibility_text);
    }

    @Override
    public void onClick(View view) {
        mPersistDoneState = false;
        mToggleGlicCallback.onClick(false);
        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_GLIC_CLICKED);
        }
        updateButtonState();
        notifyObservers(true);
    }

    @Override
    public void onTaskStateChanged(int taskId, @ActorTaskState int newState) {
        int oldButtonState = mButtonState;
        updateButtonState();
        if (mButtonState != oldButtonState) {
            notifyObservers(true);
        }
    }

    @Override
    public void onGlobalShowHide(boolean isOpened) {
        if (!isOpened) {
            if (mIsPanelOpen) {
                mIsPanelOpen = false;
                notifyObservers(true);
            }
            return;
        }
        updateIsPanelOpen();
    }
}
