// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.actor.ActorKeyedServiceFactory;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorTextButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnKeyboardFocusHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlobalShowHideObserver;
import org.chromium.chrome.browser.glic.GlicPrefNames;
import org.chromium.chrome.browser.glic.GlicTaskMenuCoordinator;
import org.chromium.chrome.browser.glic.GlicUtils;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTracker;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.widget.RectProvider;

import java.util.List;
import java.util.function.Supplier;

/** Coordinator for the trailing buttons on the tab strip. */
@NullMarked
public class StripLayoutTrailingButtonsCoordinator {
    /** Observer for changes to the trailing buttons layout state. */
    public interface StripLayoutTrailingButtonsObserver {
        /** Called when the trailing buttons layout footprint (e.g. width or visibility) changes. */
        void onTrailingButtonsLayoutStateChanged();
    }

    // TODO(crbug.com/505850223): Move Glic (+ MSB) constants to a dimens.xml
    // Glic button constants.
    private static final float GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP = 3.f;
    private static final float GLIC_BUTTON_BACKGROUND_WIDTH_DP = 42.f;
    private static final float GLIC_BUTTON_BACKGROUND_HEIGHT_DP = 32.f;
    private static final float GLIC_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY = 0.30f;
    private static final float GLIC_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY = 0.20f;
    private static final float GLIC_BUTTON_UNFOCUSED_OPACITY = 0.65f;
    // Total vertical margin (Tab Strip Height(40dp) - Glic Background Height(32dp) = 8dp).
    public static final float GLIC_BUTTON_MARGIN_HEIGHT_DP = 8.f;
    public static final float GLIC_BUTTON_START_PADDING_DP = 6.f;
    private static final float GLIC_ICON_WIDTH_DP = 16.f;
    public static final float GLIC_ICON_TEXT_PADDING_DP = 4.f;
    private static final float GLIC_BUTTON_STANDARD_END_PADDING_DP = 10.f;
    private static final float GLIC_BUTTON_SHORTENED_END_PADDING_DP = 4.f;
    private static final float GLIC_DISMISS_ICON_WIDTH_DP = 24.f;
    private static final float GLIC_DISMISS_BUTTON_Y_OFFSET_DP = 7.f;
    private static final float GLIC_DISMISS_BUTTON_CLICK_SLOP_DP =
            (StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE - GLIC_DISMISS_ICON_WIDTH_DP)
                    / 2;
    public static final float GLIC_BUTTON_CORNER_RADIUS_DP = 10.f;
    public static final float GLIC_BUTTON_INNER_CORNER_RADIUS_DP = 2.f;
    private static final float GLIC_ACTOR_BUTTON_GAP_DP = 2.f;
    private static final float GLIC_ACTOR_TEXT_HIDE_THRESHOLD_DP = 700.f;

    // Default horizontal slop for Glic buttons. This value is used as a baseline and is manually
    // adjusted in #updateTouchTargetInsets to ensure a 48dp touch target for the collapsed Glic and
    // Glic Actor buttons without causing overlap in the 2dp gap between them.
    private static final float GLIC_BUTTON_CLICK_SLOP_DP = 8.f;

    // Touch target horizontal slop adjustments for the collapsed Glic and Glic Actor buttons.
    // The base horizontal click slop is 8dp (GLIC_BUTTON_CLICK_SLOP_DP).
    //
    // To achieve the desired 48dp touch target for each 42dp wide button without causing an
    // overlap in the 2dp gap between them, the slop values are distributed non-uniformly:
    // The 2dp gap is completely allocated to the Glic button's right slop (8 - 6 = 2dp).
    // The Glic Actor button has 0dp left slop (8 - 8 = 0dp).
    // The remaining width requirements are met by the Glic button's left slop (8 - 4 = 4dp)
    // and the Glic Actor button's right slop (8 - 2 = 6dp).
    //
    // Glic button target: 4dp left slop + 42dp width + 2dp right slop = 48dp.
    // Glic Actor button target: 0dp left slop + 42dp width + 6dp right slop = 48dp.
    private static final float GLIC_COLLAPSED_LEFT_SLOP_ADJUSTMENT_DP = 4.f;
    private static final float GLIC_COLLAPSED_RIGHT_SLOP_ADJUSTMENT_DP = 6.f;
    private static final float GLIC_ACTOR_LEFT_SLOP_ADJUSTMENT_DP = 8.f;
    private static final float GLIC_ACTOR_RIGHT_SLOP_ADJUSTMENT_DP = 2.f;

    private static final int ANIM_BUTTONS_FADE_MS = 150;

    // Core Dependencies
    private final Context mContext;
    private final LayoutUpdateHost mUpdateHost;
    private final LayoutRenderHost mRenderHost;
    private final WindowAndroid mWindowAndroid;

    // Configuration & Delegates
    private final StripLayoutTrailingButtonsObserver mObserver;
    private final float mDensity;
    private final float mStripEndPadding;
    private final Callback<Boolean> mGlicClickHandler;
    private final @Nullable GlicKeyedService mGlicKeyedService;
    private final @Nullable GlobalShowHideObserver mGlicUiObserver;
    private final @Nullable ChromeAndroidTaskTracker mTaskTracker;
    private final Supplier<Boolean> mIsIncognitoSupplier;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;

    // Lifecycle & Caching Objects
    private @Nullable Profile mProfile;
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable LayerTitleCache mLayerTitleCache;

    // UI Components
    private @Nullable TintedCompositorTextButton mGlicButton;
    private @Nullable TintedCompositorButton mGlicDismissNudgeButton;
    private @Nullable TintedCompositorTextButton mGlicActorButton;
    private @Nullable GlicButtonContextMenuCoordinator mGlicButtonContextMenuCoordinator;
    private @Nullable GlicTaskMenuCoordinator mGlicTaskMenuCoordinator;
    private final View mToolbarControlContainer;

    // Layout & State Parameters
    private float mWidth;
    private float mRightPadding;
    private float mLeftPadding;
    private float mTopPadding;
    private boolean mIsGlicUiVisible;
    private boolean mIsMsbVisible;

    /**
     * Creates the trailing buttons coordinator.
     *
     * @param context The {@link Context} for constructing the button.
     * @param updateHost The {@link LayoutUpdateHost} for requesting handles layout.
     * @param renderHost The {@link LayoutRenderHost} for requesting renders.
     * @param windowAndroid The {@link WindowAndroid} for the activity.
     * @param glicClickHandler The {@link Callback<Boolean>} to execute on Glic button click.
     * @param density The display density.
     * @param stripEndPadding The end padding of the tab strip.
     * @param toolbarControlContainer The view containing toolbar controls.
     * @param keyboardFocusHandler The {@link StripLayoutViewOnKeyboardFocusHandler} for the button.
     * @param isAppInDesktopWindow Whether the app is in a desktop window.
     * @param isTopResumedActivity Whether the app is the top resumed activity.
     * @param glicKeyedService The {@link GlicKeyedService} for observing Glic UI state.
     * @param taskTracker The {@link ChromeAndroidTaskTracker} for tracking tasks.
     * @param observer The {@link StripLayoutTrailingButtonsObserver} for layout state changes.
     */
    public StripLayoutTrailingButtonsCoordinator(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            WindowAndroid windowAndroid,
            Callback<Boolean> glicClickHandler,
            float density,
            float stripEndPadding,
            View toolbarControlContainer,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            boolean isAppInDesktopWindow,
            boolean isTopResumedActivity,
            @Nullable GlicKeyedService glicKeyedService,
            @Nullable ChromeAndroidTaskTracker taskTracker,
            Supplier<Boolean> isIncognitoSupplier,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            StripLayoutTrailingButtonsObserver observer) {
        mContext = context;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;
        mGlicClickHandler = glicClickHandler;
        mDensity = density;
        mStripEndPadding = stripEndPadding;
        mGlicKeyedService = glicKeyedService;
        mTaskTracker = taskTracker;
        mIsIncognitoSupplier = isIncognitoSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mObserver = observer;
        mWindowAndroid = windowAndroid;
        mToolbarControlContainer = toolbarControlContainer;

        if (mGlicKeyedService != null) {
            mGlicUiObserver = this::updateIsPanelOpen;
            mGlicKeyedService.addGlobalShowHideObserver(mGlicUiObserver);
        } else {
            mGlicUiObserver = null;
        }

        StripLayoutViewOnClickHandler glicClickHandlerOnButton =
                (time, view, motionEventButtonState, modifiers) ->
                        mGlicClickHandler.onResult(/* result= */ false);

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.GLIC)
                && AndroidSidePanelEnabledFn.isEnabled()) {
            mGlicDismissNudgeButton =
                    new TintedCompositorButton(
                            mContext,
                            /* incognito= */ false,
                            ButtonType.GLIC_DISMISS_NUDGE,
                            /* parentView= */ null,
                            GLIC_DISMISS_ICON_WIDTH_DP,
                            GLIC_DISMISS_ICON_WIDTH_DP,
                            /* tooltipHandler= */ null,
                            (time, view, motionEventButtonState, modifiers) -> {
                                handleGlicDismissNudgeButtonClick();
                            },
                            keyboardFocusHandler,
                            R.drawable.btn_tab_close_normal,
                            Resources.ID_NULL,
                            GLIC_DISMISS_BUTTON_CLICK_SLOP_DP,
                            /* hasLongClickAction= */ false);

            mGlicDismissNudgeButton.setDrawY(GLIC_DISMISS_BUTTON_Y_OFFSET_DP);
            mGlicDismissNudgeButton.setVisible(false);
            mGlicDismissNudgeButton.setAccessibilityDescription(
                    mContext.getString(
                            R.string.accessibility_tabstrip_btn_close_tab,
                            mContext.getString(R.string.glic_button_entrypoint_label)));
            @ColorInt
            int dismissIconDefaultColor = SemanticColorUtils.getDefaultIconColor(mContext);
            mGlicDismissNudgeButton.setTint(dismissIconDefaultColor);

            // TODO(crbug.com/491225976): Add accessibility string

            mGlicButton =
                    new TintedCompositorTextButton(
                            mContext,
                            /* incognito= */ false,
                            ButtonType.GLIC,
                            /* parentView= */ null,
                            GLIC_BUTTON_BACKGROUND_WIDTH_DP,
                            GLIC_BUTTON_BACKGROUND_HEIGHT_DP,
                            (tooltipText) -> mToolbarControlContainer.setTooltipText(tooltipText),
                            glicClickHandlerOnButton,
                            keyboardFocusHandler,
                            R.drawable.ic_spark_4c_16dp,
                            GLIC_BUTTON_CLICK_SLOP_DP,
                            /* hasLongClickAction= */ true,
                            mGlicDismissNudgeButton);

            mGlicButtonContextMenuCoordinator = new GlicButtonContextMenuCoordinator(mContext);

            mGlicButton.setDrawY(GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP);
            mGlicButton.setVisible(false);

            @ColorInt
            int backgroundDefaultColor = SemanticColorUtils.getColorSurfaceContainerLow(mContext);

            @ColorInt
            int backgroundHoverColor =
                    ColorUtils.setAlphaComponentWithFloat(
                            SemanticColorUtils.getColorPrimary(mContext),
                            GLIC_BUTTON_HOVER_BACKGROUND_DEFAULT_OPACITY);

            @ColorInt
            int backgroundPressedColor =
                    ColorUtils.setAlphaComponentWithFloat(
                            SemanticColorUtils.getColorPrimary(mContext),
                            GLIC_BUTTON_HOVER_BACKGROUND_PRESSED_OPACITY);

            mGlicButton.setBackgroundTint(
                    backgroundDefaultColor,
                    backgroundHoverColor,
                    backgroundPressedColor,
                    backgroundPressedColor);

            mGlicButton.setTint(SemanticColorUtils.getDefaultIconColor(mContext));

            mGlicButton.setAccessibilityDescription(
                    mContext.getString(R.string.glic_tab_strip_button_tooltip));

            mGlicButton.setText(
                    mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label));

            mGlicActorButton =
                    new TintedCompositorTextButton(
                            mContext,
                            /* incognito= */ false,
                            ButtonType.GLIC_ACTOR,
                            /* parentView= */ null,
                            GLIC_BUTTON_BACKGROUND_WIDTH_DP,
                            GLIC_BUTTON_BACKGROUND_HEIGHT_DP,
                            /* tooltipHandler= */ null,
                            (time, view, motionEventButtonState, modifiers) ->
                                    toggleActorTaskMenu(),
                            keyboardFocusHandler,
                            R.drawable.ic_arrow_selector_spark_16dp,
                            GLIC_BUTTON_CLICK_SLOP_DP,
                            /* hasLongClickAction= */ false,
                            /* dismissButton= */ null);

            mGlicActorButton.setDrawY(GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP);
            mGlicActorButton.setVisible(false);

            mGlicActorButton.setBackgroundTint(
                    backgroundDefaultColor,
                    backgroundHoverColor,
                    backgroundPressedColor,
                    backgroundPressedColor);

            mGlicActorButton.setTint(SemanticColorUtils.getDefaultIconColor(mContext));

            // TODO(crbug.com/491225976): Add accessibility string
        }

        updateGlicButtonOpacity(isAppInDesktopWindow, isTopResumedActivity);
    }

    /** Destroys the coordinator and unregisters observers. */
    public void destroy() {
        if (mGlicKeyedService != null && mGlicUiObserver != null) {
            mGlicKeyedService.removeGlobalShowHideObserver(mGlicUiObserver);
        }
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }
        if (mGlicButtonContextMenuCoordinator != null) {
            mGlicButtonContextMenuCoordinator.dismiss();
            mGlicButtonContextMenuCoordinator = null;
        }
        if (mGlicTaskMenuCoordinator != null) {
            mGlicTaskMenuCoordinator.dismiss();
            mGlicTaskMenuCoordinator = null;
        }
    }

    /**
     * Registers a pref observer for Glic button changes when the profile is available.
     *
     * @param profile The {@link Profile} to observe.
     */
    public void onProfileAvailable(Profile profile) {
        if (mProfile == profile) return;

        mProfile = profile;
        if (mPrefChangeRegistrar != null) {
            mPrefChangeRegistrar.destroy();
            mPrefChangeRegistrar = null;
        }

        mPrefChangeRegistrar = new PrefChangeRegistrar(UserPrefs.get(profile));
        mPrefChangeRegistrar.addObserver(
                GlicPrefNames.GLIC_PINNED_TO_TABSTRIP, this::onGlicPrefChanged);

        onGlicPrefChanged();
        updateIsPanelOpen();
    }

    private void updateIsPanelOpen() {
        if (mProfile == null || mGlicKeyedService == null || mTaskTracker == null) return;
        Activity activity = ContextUtils.activityFromContext(mContext);
        if (activity == null) return;
        var task = mTaskTracker.get(activity.getTaskId());
        if (task == null) return;
        long browserWindowPtr = task.getNativeBrowserWindowPtr(mProfile, activity);
        boolean isOpened = false;
        if (browserWindowPtr != 0) {
            isOpened = mGlicKeyedService.isPanelShowingForBrowser(browserWindowPtr);
        }

        mIsGlicUiVisible = isOpened;
        if (mGlicButton != null) {
            mGlicButton.setHighlighted(isOpened);
            mRenderHost.requestRender();
        }
    }

    private void onGlicPrefChanged() {
        if (mGlicButton == null) return;
        boolean shouldGlicBeVisible = shouldGlicBeVisible();

        if (mGlicButton.isVisible() != shouldGlicBeVisible) {
            mGlicButton.setVisible(shouldGlicBeVisible);
            updateGlicButtonPosition();
            mObserver.onTrailingButtonsLayoutStateChanged();
            mUpdateHost.requestUpdate();
        }
    }

    private void handleGlicDismissNudgeButtonClick() {
        setGlicButtonText(
                mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                /* isActor= */ false);
        setGlicDismissNudgeButtonVisible(false);
    }

    /** Returns the Glic button instance. */
    public @Nullable TintedCompositorTextButton getGlicButton() {
        return mGlicButton;
    }

    /** Returns the Glic actor button instance. */
    public @Nullable TintedCompositorTextButton getGlicActorButton() {
        return mGlicActorButton;
    }

    /**
     * Populates the given list with virtual views for accessibility events.
     *
     * @param views A list of virtual views to append the trailing buttons to.
     */
    public void getVirtualViews(List<VirtualView> views) {
        if (isGlicButtonVisible()) {
            views.add(mGlicButton);
        }
        if (isGlicDismissNudgeButtonVisible()) {
            views.add(mGlicDismissNudgeButton);
        }
        if (isGlicActorButtonVisible()) {
            views.add(mGlicActorButton);
        }
    }

    /**
     * Handles strip size changes.
     *
     * @param width The full width of the strip layout.
     * @param rightPadding The padding on the right of the strip layout.
     * @param leftPadding The padding on the left of the strip layout.
     * @param topPadding The padding on the top of the strip layout.
     */
    public void onSizeChanged(
            float width, float rightPadding, float leftPadding, float topPadding) {
        if (mWidth == width
                && mRightPadding == rightPadding
                && mLeftPadding == leftPadding
                && mTopPadding == topPadding) {
            return;
        }
        mWidth = width;
        mRightPadding = rightPadding;
        mLeftPadding = leftPadding;
        mTopPadding = topPadding;

        updateActorButtonState();

        updateGlicButtonPosition();
        // Dismiss trailing buttons' menus, similar to how the app menu is dismissed on
        // orientation change
        dismissTrailingButtonsMenu();
    }

    private void updateActorButtonState() {
        if (mWidth < GLIC_ACTOR_TEXT_HIDE_THRESHOLD_DP) {
            setGlicButtonText(null, /* isActor= */ true);
        } else {
            // TODO(crbug.com/501156753): Once the JNI is available, query the state here to restore
            // the text if the actor button should have text.
        }
    }

    /** Sets the cache used for generating textures for the trailing buttons. */
    public void setLayerTitleCache(@Nullable LayerTitleCache titleCache) {
        mLayerTitleCache = titleCache;
        if (mGlicButton != null) {
            updateButtonTextProperties(mGlicButton);
        }
        if (mGlicActorButton != null) {
            updateButtonTextProperties(mGlicActorButton);
        }
    }

    /** Returns true if the trailing buttons' menus are showing. */
    public boolean isMenuShowing() {
        return (mGlicButtonContextMenuCoordinator != null
                        && mGlicButtonContextMenuCoordinator.isShowing())
                || (mGlicTaskMenuCoordinator != null && mGlicTaskMenuCoordinator.isShowing());
    }

    /** Dismisses the trailing buttons' menus if they are showing. */
    public void dismissTrailingButtonsMenu() {
        if (mGlicButtonContextMenuCoordinator != null) {
            mGlicButtonContextMenuCoordinator.dismiss();
        }
        if (mGlicTaskMenuCoordinator != null) {
            mGlicTaskMenuCoordinator.dismiss();
        }
    }

    private void toggleActorTaskMenu() {
        if (mGlicTaskMenuCoordinator != null && mGlicTaskMenuCoordinator.isShowing()) {
            mGlicTaskMenuCoordinator.dismiss();
            return;
        }

        if (mProfile == null || mGlicActorButton == null) return;
        var actorService = ActorKeyedServiceFactory.getForProfile(mProfile);
        if (actorService == null) return;

        List<ActorTask> tasks = actorService.getActiveTasks();
        if (tasks.isEmpty()) return;

        RectProvider anchorRectProvider = new RectProvider();
        mGlicActorButton.getAnchorRect(anchorRectProvider.getRect());
        StripLayoutUtils.getAdjustedAnchorRect(
                mContext,
                mToolbarControlContainer,
                mProfile.isOffTheRecord(),
                mTopPadding,
                anchorRectProvider);

        // TabModelSelector is pulled lazily via supplier. This is safe from race conditions because
        // Glic buttons require an initialized profile to be displayed/interacted with.
        if (mGlicTaskMenuCoordinator == null) {
            mGlicTaskMenuCoordinator =
                    new GlicTaskMenuCoordinator(
                            mContext, mTabModelSelectorSupplier, mGlicClickHandler::onResult);
        }
        mGlicTaskMenuCoordinator.show(
                anchorRectProvider, mToolbarControlContainer.getRootView(), tasks);
    }

    /**
     * Shows the trailing button context menu.
     *
     * @param activity The current {@link Activity}.
     * @param tabWidthDp The current tab width in DP.
     */
    public void showMenu(Activity activity, float tabWidthDp) {
        if (mGlicButtonContextMenuCoordinator == null || mProfile == null || mGlicButton == null) {
            return;
        }

        RectProvider anchorRectProvider = new RectProvider();
        mGlicButton.getAnchorRect(anchorRectProvider.getRect());

        StripLayoutUtils.getAdjustedAnchorRect(
                mContext,
                mToolbarControlContainer,
                mProfile.isOffTheRecord(),
                mTopPadding,
                anchorRectProvider);

        mGlicButtonContextMenuCoordinator.showMenu(
                anchorRectProvider, activity, mProfile, tabWidthDp);
    }

    @VisibleForTesting
    /* package */ void setGlicButtonText(@Nullable String text, boolean isActor) {
        TintedCompositorTextButton button = isActor ? mGlicActorButton : mGlicButton;
        if (button == null || TextUtils.equals(button.getText(), text)) return;

        button.setText(text);
        updateButtonTextProperties(button);
    }

    private void updateButtonTextProperties(TintedCompositorTextButton button) {
        String text = button.getText();

        if (mLayerTitleCache != null && !TextUtils.isEmpty(text)) {
            button.setTextResourceId(
                    mLayerTitleCache.getUpdatedGlicButtonText(
                            text, /* isActor= */ button == mGlicActorButton));
        } else {
            button.setTextResourceId(Resources.ID_NULL);
        }

        updateGlicButtonWidth(mLayerTitleCache);
        updateGlicButtonPosition();
        mObserver.onTrailingButtonsLayoutStateChanged();
    }

    private void updateGlicButtonWidth(@Nullable LayerTitleCache titleCache) {
        if (mGlicButton != null) {
            mGlicButton.setWidth(calculateButtonWidth(mGlicButton, titleCache));
        }

        if (mGlicActorButton != null) {
            mGlicActorButton.setWidth(calculateButtonWidth(mGlicActorButton, titleCache));
        }
    }

    private float calculateButtonWidth(
            TintedCompositorTextButton button, @Nullable LayerTitleCache titleCache) {
        String text = button.getText();
        float width = GLIC_BUTTON_BACKGROUND_WIDTH_DP;

        if (!TextUtils.isEmpty(text) && titleCache != null) {
            width =
                    GLIC_BUTTON_START_PADDING_DP
                            + GLIC_ICON_WIDTH_DP
                            + GLIC_ICON_TEXT_PADDING_DP
                            + (titleCache.getButtonTextWidth(text) / mDensity);

            if (isGlicDismissNudgeButtonVisible() && button == mGlicButton) {
                width += GLIC_BUTTON_SHORTENED_END_PADDING_DP + GLIC_DISMISS_ICON_WIDTH_DP;
            } else {
                width += GLIC_BUTTON_STANDARD_END_PADDING_DP;
            }
        }

        return width;
    }

    /**
     * Updates the visibility of the model selector button.
     *
     * @param visible Whether the model selector button should be visible.
     */
    public void setModelSelectorButtonVisible(boolean visible) {
        mIsMsbVisible = visible;
        updateGlicButtonPosition();
    }

    /** Updates the position of the Glic buttons based on layout parameters. */
    public void updateGlicButtonPosition() {
        if (mGlicButton == null || mGlicDismissNudgeButton == null || mGlicActorButton == null) {
            return;
        }

        // 1. X Positions
        if (!LocalizationUtils.isLayoutRtl()) {
            float rightSideAnchor = mWidth - mRightPadding - mStripEndPadding;
            if (mIsMsbVisible) {
                rightSideAnchor -= StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE;
            }
            if (isGlicActorButtonVisible()) {
                mGlicActorButton.setDrawX(rightSideAnchor - mGlicActorButton.getWidth());
                rightSideAnchor -= mGlicActorButton.getWidth() + GLIC_ACTOR_BUTTON_GAP_DP;
            }
            mGlicButton.setDrawX(rightSideAnchor - mGlicButton.getWidth());
            if (mGlicDismissNudgeButton.isVisible()) {
                mGlicDismissNudgeButton.setDrawX(
                        rightSideAnchor
                                - GLIC_BUTTON_SHORTENED_END_PADDING_DP
                                - GLIC_DISMISS_ICON_WIDTH_DP);
            }
        } else {
            float leftSideAnchor = mLeftPadding + mStripEndPadding;
            if (mIsMsbVisible) {
                leftSideAnchor += StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE;
            }
            if (isGlicActorButtonVisible()) {
                mGlicActorButton.setDrawX(leftSideAnchor);
                leftSideAnchor += mGlicActorButton.getWidth() + GLIC_ACTOR_BUTTON_GAP_DP;
            }
            mGlicButton.setDrawX(leftSideAnchor);
            if (mGlicDismissNudgeButton.isVisible()) {
                mGlicDismissNudgeButton.setDrawX(
                        leftSideAnchor + GLIC_BUTTON_SHORTENED_END_PADDING_DP);
            }
        }

        // 2. Y Positions
        mGlicButton.setDrawY(GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP);
        mGlicDismissNudgeButton.setDrawY(GLIC_DISMISS_BUTTON_Y_OFFSET_DP);
        mGlicActorButton.setDrawY(GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP);

        // 3. Touch Targets
        updateTouchTargetInsets();
    }

    @RequiresNonNull({"mGlicButton", "mGlicDismissNudgeButton", "mGlicActorButton"})
    private void updateTouchTargetInsets() {
        // TODO(crbug.com/509585777): Implement RTL support
        if (isGlicButtonVisible()) {
            mGlicButton.setTouchTargetInsets(
                    GLIC_COLLAPSED_LEFT_SLOP_ADJUSTMENT_DP,
                    mTopPadding,
                    GLIC_COLLAPSED_RIGHT_SLOP_ADJUSTMENT_DP,
                    -mTopPadding);
        } else {
            // Revert to default uniform 8dp slop horizontally.
            mGlicButton.setTouchTargetInsets(null, mTopPadding, null, -mTopPadding);
        }
        mGlicActorButton.setTouchTargetInsets(
                GLIC_ACTOR_LEFT_SLOP_ADJUSTMENT_DP,
                mTopPadding,
                GLIC_ACTOR_RIGHT_SLOP_ADJUSTMENT_DP,
                -mTopPadding);
        mGlicDismissNudgeButton.setTouchTargetInsets(null, mTopPadding, null, -mTopPadding);
    }

    /**
     * Updates the opacity of the Glic buttons based on app focus state.
     *
     * @param isAppInDesktopWindow Whether the app is in a desktop window.
     * @param isTopResumedActivity Whether the app is the top resumed activity.
     */
    public void updateGlicButtonOpacity(
            boolean isAppInDesktopWindow, boolean isTopResumedActivity) {
        if (mGlicButton == null || mGlicActorButton == null) return;
        boolean isUnfocusedInDw = isAppInDesktopWindow && !isTopResumedActivity;
        mGlicButton.setOpacity(isUnfocusedInDw ? GLIC_BUTTON_UNFOCUSED_OPACITY : 1.0f);
        mGlicActorButton.setOpacity(isUnfocusedInDw ? GLIC_BUTTON_UNFOCUSED_OPACITY : 1.0f);
    }

    /** Returns the total width used by the trailing buttons including padding. */
    public float getTrailingButtonsWidthWithPadding() {
        float width = 0.0f;
        if (isGlicButtonVisible()) {
            width += mGlicButton.getWidth();
        }
        if (isGlicActorButtonVisible()) {
            // Add spacing gap regardless of whether primary Glic button is showing.
            width += GLIC_ACTOR_BUTTON_GAP_DP;
            width += mGlicActorButton.getWidth();
        }
        if (width > 0.0f) {
            // Add end padding and start slop to meet touch target requirements.
            width += mStripEndPadding + GLIC_BUTTON_CLICK_SLOP_DP;
        }
        return width;
    }

    /**
     * Sets the visibility of the Glic button.
     *
     * @param visible Whether the button should be visible.
     */
    public void setGlicButtonVisible(boolean visible) {
        if (mGlicButton != null) {
            mGlicButton.setVisible(visible);
        }
    }

    /** Returns whether the Glic button is currently visible. */
    @EnsuresNonNullIf("mGlicButton")
    public boolean isGlicButtonVisible() {
        return mGlicButton != null && mGlicButton.isVisible();
    }

    /**
     * Sets the visibility of the Glic dismiss nudge button.
     *
     * @param isVisible Whether the Glic dismiss nudge button should be visible.
     */
    public void setGlicDismissNudgeButtonVisible(boolean isVisible) {
        if (mGlicDismissNudgeButton == null || mGlicDismissNudgeButton.isVisible() == isVisible) {
            return;
        }

        mGlicDismissNudgeButton.setVisible(isVisible);
        updateGlicButtonWidth(mLayerTitleCache);
        updateGlicButtonPosition();
        mObserver.onTrailingButtonsLayoutStateChanged();
        mUpdateHost.requestUpdate();
    }

    /** Returns whether the Glic dismiss nudge button is currently visible. */
    @EnsuresNonNullIf("mGlicDismissNudgeButton")
    public boolean isGlicDismissNudgeButtonVisible() {
        return mGlicDismissNudgeButton != null && mGlicDismissNudgeButton.isVisible();
    }

    /**
     * Sets the visibility of the Glic actor button.
     *
     * @param visible Whether the actor button should be visible.
     */
    public void setGlicActorButtonVisible(boolean visible) {
        if (mGlicActorButton == null || mGlicActorButton.isVisible() == visible) return;

        mGlicActorButton.setVisible(visible);

        if (visible) {
            setGlicButtonText(null, /* isActor= */ false);
        } else {
            setGlicButtonText(
                    mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label),
                    /* isActor= */ false);
        }

        // TODO(crbug.com/496678704): When Actor button visibility is driven by task list, these 3
        // manual layout triggers can be removed. See similar logic exists in onGlicPrefChanged.
        updateGlicButtonPosition();
        mObserver.onTrailingButtonsLayoutStateChanged();
        mUpdateHost.requestUpdate();
    }

    /** Returns whether the Glic actor button is currently visible. */
    @EnsuresNonNullIf("mGlicActorButton")
    public boolean isGlicActorButtonVisible() {
        return mGlicActorButton != null && mGlicActorButton.isVisible();
    }

    /** Returns whether the Glic UI is currently visible (e.g. panel is open). */
    public boolean isGlicUiVisible() {
        return mIsGlicUiVisible;
    }

    /**
     * Fades out trailing buttons when the compositor buttons are set to hidden (e.g. during tab
     * drag).
     */
    public void fadeCompositorButtons(boolean visible) {
        if (mGlicButton != null) {
            float endOpacity = visible ? 1.f : 0.f;
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            mGlicButton,
                            CompositorButton.OPACITY,
                            mGlicButton.getOpacity(),
                            endOpacity,
                            ANIM_BUTTONS_FADE_MS)
                    .start();
        }
    }

    /**
     * Determines whether the Glic button should be visible in the tab strip.
     *
     * @return true if the Glic button should be visible.
     */
    public boolean shouldGlicBeVisible() {
        if (mGlicButton == null || mIsIncognitoSupplier.get() || mProfile == null) {
            return false;
        }
        return GlicUtils.isButtonPinnedToTabStrip(mProfile);
    }

    /**
     * Determines whether the Glic actor button should be visible in the tab strip.
     *
     * @return true if the Glic actor button should be visible.
     */
    public boolean shouldGlicActorBeVisible() {
        if (!shouldGlicBeVisible() || mGlicActorButton == null) {
            return false;
        }

        // TODO(crbug.com/496678704): Query the state to check for active actor tasks
        return mGlicActorButton.isVisible();
    }

    /**
     * Handles down touch events.
     *
     * @param x The x coordinate of the event.
     * @param y The y coordinate of the event.
     * @param buttons The buttons pressed.
     * @return true if the event was handled.
     */
    public boolean onDown(float x, float y, int buttons) {
        if (mGlicButton != null && mGlicButton.onDown(x, y, buttons)) {
            return true;
        } else if (mGlicActorButton != null && mGlicActorButton.onDown(x, y, buttons)) {
            return true;
        }
        return false;
    }

    /**
     * Handles up or cancel touch events.
     *
     * @return true if the event was handled.
     */
    public boolean onUpOrCancel() {
        if (mGlicButton != null && mGlicButton.onUpOrCancel()) {
            mGlicClickHandler.onResult(/* result= */ false);
            return true;
        } else if (mGlicActorButton != null && mGlicActorButton.onUpOrCancel()) {
            return true;
        }
        return false;
    }

    /**
     * Handles long press touch events.
     *
     * @param x The x coordinate of the event.
     * @param y The y coordinate of the event.
     * @param tabWidthDp The current tab width in DP.
     * @return True if the event was handled and hit a trailing button.
     */
    public boolean onLongPress(float x, float y, float tabWidthDp) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) return false;
        if (mGlicButton != null && mGlicButton.checkClickedOrHovered(x, y)) {
            showMenu(activity, tabWidthDp);
            // Clear the pressed state so a click isn't triggered in addition to the long press.
            mGlicButton.setPressed(false);
            return true;
        } else if (mGlicActorButton != null && mGlicActorButton.checkClickedOrHovered(x, y)) {
            return true;
        }
        return false;
    }

    /**
     * Handles hover move events on the trailing buttons.
     *
     * @param x The x coordinate of the hover event.
     * @param y The y coordinate of the hover event.
     */
    public boolean onHoverEvent(float x, float y) {
        boolean glicHovered = mGlicButton != null && mGlicButton.checkClickedOrHovered(x, y);
        boolean actorHovered =
                mGlicActorButton != null && mGlicActorButton.checkClickedOrHovered(x, y);
        boolean renderNeeded = false;

        if (mGlicButton != null && glicHovered != mGlicButton.isHovered()) {
            mGlicButton.setHovered(glicHovered);
            renderNeeded = true;
        }
        if (mGlicActorButton != null && actorHovered != mGlicActorButton.isHovered()) {
            mGlicActorButton.setHovered(actorHovered);
            renderNeeded = true;
        }

        if (renderNeeded) {
            mRenderHost.requestRender();
        }
        return glicHovered || actorHovered;
    }

    /** Clears hover states on the trailing buttons. */
    public void onHoverExit() {
        boolean renderNeeded = false;
        if (mGlicButton != null && mGlicButton.isHovered()) {
            mGlicButton.setHovered(false);
            renderNeeded = true;
        }
        if (mGlicActorButton != null && mGlicActorButton.isHovered()) {
            mGlicActorButton.setHovered(false);
            renderNeeded = true;
        }
        if (renderNeeded) {
            mRenderHost.requestRender();
        }
    }

    /**
     * Checks if the trailing buttons are clicked or hovered.
     *
     * @param x The x coordinate.
     * @param y The y coordinate.
     * @return True if the event coordinates hit a trailing button.
     */
    public boolean checkClickedOrHovered(float x, float y) {
        if (mGlicButton != null && mGlicButton.checkClickedOrHovered(x, y)) {
            return true;
        } else if (mGlicActorButton != null && mGlicActorButton.checkClickedOrHovered(x, y)) {
            return true;
        }
        return false;
    }

    /**
     * Handles drag events.
     *
     * @param x The x coordinate of the event.
     * @param y The y coordinate of the event.
     */
    public void drag(float x, float y) {
        if (mGlicButton != null) {
            mGlicButton.drag(x, y);
        }
        if (mGlicActorButton != null) {
            mGlicActorButton.drag(x, y);
        }
    }

    /**
     * Set state for a click event.
     *
     * @param time The time of the click in ms.
     * @param x The x coordinate of the click event.
     * @param y The y coordinate of the click event.
     * @param buttons State of all buttons that are pressed.
     * @param modifiers State of all modifiers.
     * @param tabWidthDp The current tab width in DP.
     * @return Whether the event was handled.
     */
    public boolean click(
            long time, float x, float y, int buttons, int modifiers, float tabWidthDp) {
        if (mGlicButton != null && mGlicButton.checkClickedOrHovered(x, y)) {
            if (MotionEventUtils.isSecondaryClick(buttons)) {
                Activity activity = mWindowAndroid.getActivity().get();
                if (activity != null) {
                    showMenu(activity, tabWidthDp);
                    return true;
                }
            } else if (mGlicButton.click(x, y, buttons)) {
                mGlicButton.handleClick(time, buttons, modifiers);
                return true;
            }
        } else if (mGlicActorButton != null && mGlicActorButton.checkClickedOrHovered(x, y)) {
            if (mGlicActorButton.click(x, y, buttons)) {
                mGlicActorButton.handleClick(time, buttons, modifiers);
                return true;
            }
        }
        return false;
    }
}
