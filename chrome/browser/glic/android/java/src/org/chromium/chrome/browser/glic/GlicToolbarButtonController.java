// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.glic.GlicButtonStateController.ButtonState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
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

import java.util.List;
import java.util.function.Supplier;

/** Defines a toolbar button to open the Glic bottom sheet. */
@NullMarked
public class GlicToolbarButtonController extends BaseButtonDataProvider {
    public static final int ACTION_CHIP_COLLAPSE_DELAY_MS = 30000;

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

    private final Activity mActivity;
    private final GlicButtonDelegate mToggleGlicCallback;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final ButtonSpec mDefaultSpec;
    private final ButtonSpec mWorkingSpec;
    private final ButtonSpec mReviewSpec;
    private final ButtonSpec mDoneSpec;
    private final GlicButtonStateController mStateController;

    private @Nullable GlicTaskMenuCoordinator mTaskMenuCoordinator;

    /**
     * @param activity The Android activity.
     * @param activeTabSupplier The currently active tab.
     * @param toggleGlicCallback Callback to run when the button is clicked to open Glic.
     * @param trackerSupplier Supplier for the current profile tracker.
     * @param taskSupplier Supplier for the ChromeAndroidTask.
     * @param browserControlsVisibilityManager Manager for browser controls.
     * @param tabModelSelectorSupplier Supplier for the TabModelSelector.
     */
    public GlicToolbarButtonController(
            Activity activity,
            Supplier<@Nullable Tab> activeTabSupplier,
            GlicButtonDelegate toggleGlicCallback,
            Supplier<@Nullable Tracker> trackerSupplier,
            Supplier<@Nullable ChromeAndroidTask> taskSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier) {
        super(
                activeTabSupplier,
                /* modalDialogManager= */ null,
                new ButtonSpec.Builder(
                                AppCompatResources.getDrawable(activity, R.drawable.ic_spark_24dp),
                                activity.getString(
                                        R.string.glic_button_entrypoint_ask_gemini_label),
                                /* supportsTinting= */ true)
                        .setButtonVariant(AdaptiveToolbarButtonVariant.GLIC)
                        .build());
        mActivity = activity;
        mToggleGlicCallback = toggleGlicCallback;
        mTrackerSupplier = trackerSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mDefaultSpec = mButtonData.getButtonSpec();
        Drawable collapsedDrawable =
                AppCompatResources.getDrawable(activity, R.drawable.glic_dirty_dot_spark);

        mStateController =
                new GlicButtonStateController(
                        activity,
                        (state, isPanelOpen) -> notifyObservers(true),
                        taskSupplier,
                        browserControlsVisibilityManager);

        mWorkingSpec = createWorkingSpec(activity);
        mReviewSpec =
                new ButtonSpec.Builder(createReviewSpec())
                        .setCollapsedDrawable(collapsedDrawable)
                        .build();
        mDoneSpec =
                new ButtonSpec.Builder(createDoneSpec())
                        .setCollapsedDrawable(collapsedDrawable)
                        .build();
    }

    private ButtonSpec createReviewSpec() {
        return new ButtonSpec.Builder(mDefaultSpec)
                .setActionChipLabelResId(R.string.glic_button_status_review)
                .setShouldSuppressCpa(true)
                .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                .setActionChipBackgroundColorResId(R.attr.colorSecondaryContainer)
                .setActionChipTextColorResId(R.attr.colorOnSecondaryContainer)
                .build();
    }

    private ButtonSpec createDoneSpec() {
        return new ButtonSpec.Builder(mDefaultSpec)
                .setActionChipLabelResId(R.string.glic_button_status_done)
                .setShouldSuppressCpa(true)
                .setActionChipCollapseDelayMs(ACTION_CHIP_COLLAPSE_DELAY_MS)
                .setActionChipBackgroundColorResId(R.attr.colorTertiaryContainer)
                .setActionChipTextColorResId(R.attr.colorOnTertiaryContainer)
                .build();
    }

    private ButtonSpec createWorkingSpec(Context context) {
        Drawable sparkIcon = AppCompatResources.getDrawable(context, R.drawable.ic_spark_24dp);
        Drawable layerDrawable = GlicUiHelper.createWorkingDrawable(context, sparkIcon);
        return new ButtonSpec.Builder(mDefaultSpec)
                .setDrawable(layerDrawable)
                .setShouldSuppressCpa(true)
                .build();
    }

    @Override
    protected boolean shouldShowButton(@Nullable Tab tab) {
        if (tab == null || tab.isOffTheRecord() || UrlUtilities.isNtpUrl(tab.getUrl())) {
            return false;
        }
        // TODO(crbug.com/499354469): Add proper checks for glic availability.
        if (!AdaptiveToolbarFeatures.isGlicEnabledForProfile(tab.getProfile())) {
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

        assumeNonNull(tab);
        mStateController.updateObservations(tab.getProfile());
        mStateController.updateButtonState();

        ButtonSpec desiredSpec = mDefaultSpec;
        switch (mStateController.getButtonState()) {
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
                new ButtonSpec.Builder(desiredSpec)
                        .setIsChecked(mStateController.isPanelOpen())
                        .build());

        mButtonData.setEnabled(true);
        return buttonData;
    }

    @Override
    public void destroy() {
        mStateController.destroy();
        if (mTaskMenuCoordinator != null) {
            mTaskMenuCoordinator.dismiss();
            mTaskMenuCoordinator = null;
        }
        super.destroy();
    }

    @Override
    protected @Nullable IphCommandBuilder getIphCommandBuilder(Tab tab) {
        return new IphCommandBuilder(
                mActivity.getResources(),
                FeatureConstants.GLIC_PROMO_ANDROID_FEATURE,
                R.string.iph_glic_promo_text,
                R.string.iph_glic_promo_accessibility_text);
    }

    @Override
    public void onClick(View view) {
        mStateController.setPersistDoneState(false);

        if (mTaskMenuCoordinator != null && mTaskMenuCoordinator.isShowing()) {
            mTaskMenuCoordinator.dismiss();
            return;
        }

        List<ActorTask> tasks = mStateController.getActiveTasks();
        if (tasks != null) {
            Tab currentTab = mActiveTabSupplier.get();
            boolean isOnActingTab =
                    currentTab != null
                            && mStateController.getActiveTaskIdOnTab(currentTab.getId()) != null;

            if (!isOnActingTab && !tasks.isEmpty()) {
                if (mTaskMenuCoordinator == null) {
                    mTaskMenuCoordinator =
                            new GlicTaskMenuCoordinator(
                                    mActivity, mTabModelSelectorSupplier, mToggleGlicCallback);
                }
                mTaskMenuCoordinator.show(view, tasks);
                return;
            }
        }

        mToggleGlicCallback.onClick(false);
        Tracker tracker = mTrackerSupplier.get();
        if (tracker != null) {
            tracker.notifyEvent(EventConstants.ADAPTIVE_TOOLBAR_CUSTOMIZATION_GLIC_CLICKED);
        }
        mStateController.updateButtonState();
        notifyObservers(true);
    }

    void onGlobalShowHideForTesting() {
        mStateController.onGlobalShowHide();
    }
}
