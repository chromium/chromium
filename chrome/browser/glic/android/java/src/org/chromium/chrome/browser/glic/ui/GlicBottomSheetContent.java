// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic.ui;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorKeyedService;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetContent;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.GlowSpec;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;

import java.util.List;

/**
 * Concrete implementation of {@link TabBottomSheetContent} for the Glic feature. Handles background
 * glow override and dynamic profile-based active task checking to handle Glic's suppression logic.
 */
@NullMarked
public class GlicBottomSheetContent extends TabBottomSheetContent {
    private final Profile mProfile;

    private @Nullable Runnable mOnNativeFreAccepted;
    private @Nullable Runnable mDestroyCallback;
    private @Nullable GlicBottomSheetFreCoordinator mFreCoordinator;
    private @Nullable View mFreScrollView;

    /**
     * Constructor.
     *
     * @param contentView The inflated view for the bottom sheet.
     * @param defaultHeightRatio The default height ratio for the bottom sheet.
     * @param fullHeightRatio The full height ratio for the bottom sheet.
     * @param backgroundColor The background color for the bottom sheet.
     * @param peekViewHeight The height of the peek view in pixels.
     * @param peekViewContainerId The resource ID for the peek view container.
     * @param onBackPressed Callback run when the back button/swipe is triggered.
     * @param profile The active user profile for actor task verification.
     */
    public GlicBottomSheetContent(
            View contentView,
            float defaultHeightRatio,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            Runnable onBackPressed,
            Profile profile) {
        super(
                contentView,
                defaultHeightRatio,
                fullHeightRatio,
                backgroundColor,
                peekViewHeight,
                peekViewContainerId,
                onBackPressed);
        mProfile = profile;
    }

    void setDestroyCallback(Runnable destroyCallback) {
        mDestroyCallback = destroyCallback;
    }

    void mountNativeFre(SettingsCustomTabLauncher customTabLauncher, Runnable onNativeFreAccepted) {
        if (mFreCoordinator != null) return;

        ViewGroup expanded = TabBottomSheetUtils.getExpandedContentGroup(getContentView());
        assert expanded != null;
        if (expanded == null) return;
        mOnNativeFreAccepted = onNativeFreAccepted;
        mFreCoordinator =
                new GlicBottomSheetFreCoordinator(
                        getContentView().getContext(),
                        expanded,
                        GlicEnabling.isEnterpriseAccount(mProfile),
                        getSheetBackgroundColorOverride(),
                        customTabLauncher,
                        this::onFreAccepted,
                        this::onBackPressed);
        mFreScrollView = mFreCoordinator.getView().findViewById(R.id.glic_fre_scroll_view);
        setWebUiHidden(true);
    }

    private void onFreAccepted() {
        GlicBottomSheetFreCoordinator fre = mFreCoordinator;
        if (fre == null) return;
        Runnable onAccepted = mOnNativeFreAccepted;
        mOnNativeFreAccepted = null;
        mFreScrollView = null;
        fre.fadeOut(this::teardownNativeFre);
        if (onAccepted != null) onAccepted.run();
    }

    @Override
    public void destroy() {
        teardownNativeFre();
        if (mDestroyCallback != null) mDestroyCallback.run();
        mDestroyCallback = null;
        super.destroy();
    }

    void teardownNativeFre() {
        mOnNativeFreAccepted = null;
        mFreScrollView = null;
        GlicBottomSheetFreCoordinator fre = mFreCoordinator;
        mFreCoordinator = null;
        if (fre != null) {
            fre.destroy();
            setWebUiHidden(false);
        }
    }

    /** Hides the WebUI behind the FRE card from TalkBack and keyboard focus, or restores it. */
    private void setWebUiHidden(boolean hidden) {
        if (!(TabBottomSheetUtils.getWebUiContainer(getContentView()) instanceof ViewGroup webUi)) {
            return;
        }
        webUi.setImportantForAccessibility(
                hidden
                        ? View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                        : View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
        webUi.setDescendantFocusability(
                hidden ? ViewGroup.FOCUS_BLOCK_DESCENDANTS : ViewGroup.FOCUS_BEFORE_DESCENDANTS);
    }

    @Override
    public int getVerticalScrollOffset() {
        if (mFreScrollView != null) {
            return mFreScrollView.getScrollY();
        }
        return super.getVerticalScrollOffset();
    }

    boolean isNativeFreShowingForTesting() {
        return mFreCoordinator != null;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        return R.string.glic_bottom_sheet_half_height_a11y_label;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.glic_bottom_sheet_full_height_a11y_label;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.glic_bottom_sheet_closed_a11y_label;
    }

    @Override
    public @StringRes int getSheetHiddenAccessibilityStringId() {
        return R.string.glic_bottom_sheet_hidden_a11y_label;
    }

    @Override
    public @Nullable GlowSpec getSheetBackgroundGlowSpecOverride() {
        return new GlowSpec(
                getContentView().getContext().getColor(R.color.default_bg_color_blue),
                GlowSpec.ShadowSize.LONG);
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.glic_button_entrypoint_label);
    }

    @Override
    public boolean canBeSuppressed(BottomSheetContent nextContent) {
        ActorKeyedService service = ActorKeyedServiceFactory.getForProfile(mProfile);
        if (service == null) return true;

        List<ActorTask> activeTasks = service.getActiveTasks();
        if (activeTasks == null) return true;

        for (ActorTask task : activeTasks) {
            if (task.isUnderActorControl() && !task.isCompleted()) {
                return false; // Active agent task exists, prevent suppression.
            }
        }
        return true;
    }
}
