// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.FloatProperty;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

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
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.glic.GlicButtonStateController;
import org.chromium.chrome.browser.glic.GlicButtonStateController.ButtonState;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlobalShowHideObserver;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
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
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
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

    // Slop values used in #updateTouchTargetInsets to ensure at least a 48dp touch target in the
    // Glic and Glic Actor buttons.
    //
    // To achieve the desired 48dp touch target for each 42dp wide button without causing an
    // overlap in the 2dp gap between them, the slop values are distributed non-uniformly:
    // The 2dp gap is completely allocated to the Glic button's end slop (8 - 6 = 2dp).
    // The Glic Actor button has 0dp start slop (8 - 8 = 0dp).
    // The remaining width requirements are met by the Glic button's start slop (8 - 4 = 4dp)
    // and the Glic Actor button's end slop (8 - 2 = 6dp).
    //
    // Glic button target: 4dp start slop + 42dp width + 2dp end slop = 48dp.
    // Glic Actor button target: 0dp start slop + 42dp width + 6dp end slop = 48dp.
    private static final float GLIC_BUTTON_START_SLOP_DP = 4.f;
    private static final float GLIC_BUTTON_END_SLOP_DP = 6.f;
    private static final float GLIC_BUTTON_WITH_ACTOR_END_SLOP_DP = 2.f;
    private static final float GLIC_ACTOR_START_SLOP_DP = 0.f;
    private static final float GLIC_BUTTON_VERTICAL_SLOP_DP = 8.f;

    // Core Dependencies
    private final Context mContext;
    private final LayoutUpdateHost mUpdateHost;
    private final LayoutRenderHost mRenderHost;
    private final WindowAndroid mWindowAndroid;

    // Configuration & Delegates
    private final StripLayoutTrailingButtonsObserver mObserver;
    private final float mDensity;
    private final GlicButtonDelegate mGlicClickHandler;
    private final GlobalShowHideObserver mGlicUiObserver;
    private final GlicKeyedService.AllowedChangedObserver mAllowedChangedObserver =
            () -> updateTrailingButtonsState(/* animate= */ false, /* forceLayoutChanged= */ false);
    private final @Nullable ChromeAndroidTaskTracker mTaskTracker;
    private final Supplier<Boolean> mIsIncognitoSupplier;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;

    // Lifecycle & Caching Objects
    private @Nullable Profile mProfile;
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable LayerTitleCache mLayerTitleCache;
    private @Nullable GlicKeyedService mGlicKeyedService;

    // UI Components
    private @Nullable TintedCompositorTextButton mGlicButton;
    private @Nullable TintedCompositorButton mGlicDismissNudgeButton;
    private @Nullable TintedCompositorTextButton mGlicActorButton;
    private @Nullable GlicButtonContextMenuCoordinator mGlicButtonContextMenuCoordinator;
    private @Nullable GlicTaskMenuCoordinator mGlicTaskMenuCoordinator;
    private @Nullable GlicButtonStateController mStateController;
    private final View mToolbarControlContainer;

    // Layout & State Parameters
    private float mWidth;
    private float mRightPadding;
    private float mLeftPadding;
    private float mTopPadding;
    private boolean mIsGlicUiVisible;
    private boolean mIsMsbVisible;
    private int mLastGlicActorButtonState = ButtonState.DEFAULT;
    private boolean mIsUnfocusedInDw;

    // Animations
    private static final int ANIM_BUTTONS_FADE_MS = 150;
    private static final int GLIC_ANIMATION_EXPANSION_IN_MS = 500;
    private static final int GLIC_ANIMATION_EXPANSION_OUT_MS = 250;
    private static final int GLIC_ANIMATION_OPACITY_IN_MS = 300;
    private static final int GLIC_ANIMATION_OPACITY_OUT_MS = 100;
    private static final int GLIC_ANIMATION_OPACITY_DELAY_MS = 100;
    private @Nullable CompositorAnimator mGlicButtonWidthAnimator;
    private @Nullable CompositorAnimator mGlicActorButtonWidthAnimator;
    private @Nullable CompositorAnimator mGlicButtonOpacityAnimator;
    private @Nullable CompositorAnimator mGlicActorButtonOpacityAnimator;
    private @Nullable CompositorAnimator mGlicDismissButtonSlideAnimator;
    private float mDismissButtonXOffset;

    /** Property for width animations on the Glic button. */
    public static final FloatProperty<StripLayoutTrailingButtonsCoordinator> GLIC_BUTTON_WIDTH =
            new FloatProperty<>("glicButtonWidth") {
                @Override
                public void setValue(StripLayoutTrailingButtonsCoordinator object, float value) {
                    if (object.mGlicButton != null) {
                        object.mGlicButton.setWidth(value);
                        object.updateGlicButtonPosition();
                    }
                }

                @Override
                public Float get(StripLayoutTrailingButtonsCoordinator object) {
                    return object.mGlicButton != null ? object.mGlicButton.getWidth() : 0.f;
                }
            };

    /** Property for width animations on the Glic Actor button. */
    public static final FloatProperty<StripLayoutTrailingButtonsCoordinator>
            GLIC_ACTOR_BUTTON_WIDTH =
                    new FloatProperty<>("glicActorButtonWidth") {
                        @Override
                        public void setValue(
                                StripLayoutTrailingButtonsCoordinator object, float value) {
                            if (object.mGlicActorButton != null) {
                                object.mGlicActorButton.setWidth(value);
                                object.updateGlicButtonPosition();
                            }
                        }

                        @Override
                        public Float get(StripLayoutTrailingButtonsCoordinator object) {
                            return object.mGlicActorButton != null
                                    ? object.mGlicActorButton.getWidth()
                                    : 0.f;
                        }
                    };

    /** Property for slide animations on the Glic dismiss nudge button. */
    public static final FloatProperty<StripLayoutTrailingButtonsCoordinator>
            GLIC_DISMISS_BUTTON_X_OFFSET =
                    new FloatProperty<>("glicDismissButtonXOffset") {
                        @Override
                        public void setValue(
                                StripLayoutTrailingButtonsCoordinator object, float value) {
                            object.mDismissButtonXOffset = value;
                            object.updateGlicButtonPosition();
                        }

                        @Override
                        public Float get(StripLayoutTrailingButtonsCoordinator object) {
                            return object.mDismissButtonXOffset;
                        }
                    };

    /**
     * Creates the trailing buttons coordinator.
     *
     * @param context The {@link Context} for constructing the button.
     * @param updateHost The {@link LayoutUpdateHost} for requesting handles layout.
     * @param renderHost The {@link LayoutRenderHost} for requesting renders.
     * @param windowAndroid The {@link WindowAndroid} for the activity.
     * @param glicClickHandler The {@link GlicButtonDelegate} to execute on Glic button click.
     * @param density The display density.
     * @param toolbarControlContainer The view containing toolbar controls.
     * @param keyboardFocusHandler The {@link StripLayoutViewOnKeyboardFocusHandler} for the button.
     * @param isAppInDesktopWindow Whether the app is in a desktop window.
     * @param isTopResumedActivity Whether the app is the top resumed activity.
     * @param taskTracker The {@link ChromeAndroidTaskTracker} for tracking tasks.
     * @param observer The {@link StripLayoutTrailingButtonsObserver} for layout state changes.
     */
    public StripLayoutTrailingButtonsCoordinator(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            WindowAndroid windowAndroid,
            GlicButtonDelegate glicClickHandler,
            float density,
            View toolbarControlContainer,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            boolean isAppInDesktopWindow,
            boolean isTopResumedActivity,
            @Nullable ChromeAndroidTaskTracker taskTracker,
            Supplier<Boolean> isIncognitoSupplier,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            StripLayoutTrailingButtonsObserver observer) {
        mContext = context;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;
        mGlicClickHandler = glicClickHandler;
        mDensity = density;
        mTaskTracker = taskTracker;
        mIsIncognitoSupplier = isIncognitoSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mObserver = observer;
        mWindowAndroid = windowAndroid;
        mToolbarControlContainer = toolbarControlContainer;
        mGlicUiObserver = this::updateIsPanelOpen;

        StripLayoutViewOnClickHandler glicClickHandlerOnButton =
                (time, view, motionEventButtonState, modifiers) ->
                        mGlicClickHandler.onClick(
                                /* preventClose= */ false, GlicInvocationSource.TOP_CHROME_BUTTON);

        if (GlicEnabling.isEnabledByFlags() && AndroidSidePanelEnabledFn.isEnabled()) {
            mGlicDismissNudgeButton =
                    new TintedCompositorButton(
                            mContext,
                            /* incognito= */ false,
                            ButtonType.GLIC_DISMISS_NUDGE,
                            /* parentView= */ null,
                            GLIC_DISMISS_ICON_WIDTH_DP,
                            GLIC_DISMISS_ICON_WIDTH_DP,
                            (tooltipText) -> mToolbarControlContainer.setTooltipText(tooltipText),
                            (time, view, motionEventButtonState, modifiers) -> {
                                setGlicDismissNudgeButtonVisible(false);
                            },
                            keyboardFocusHandler,
                            R.drawable.btn_tab_close_normal,
                            Resources.ID_NULL,
                            GLIC_DISMISS_BUTTON_CLICK_SLOP_DP,
                            /* hasLongClickAction= */ false);

            mGlicDismissNudgeButton.setDrawY(GLIC_DISMISS_BUTTON_Y_OFFSET_DP);
            mGlicDismissNudgeButton.setVisible(false);
            mGlicDismissNudgeButton.setAccessibilityDescription(
                    mContext.getString(R.string.tooltip_glic_close));
            @ColorInt
            int dismissIconDefaultColor = SemanticColorUtils.getDefaultIconColor(mContext);
            mGlicDismissNudgeButton.setTint(dismissIconDefaultColor);

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
                            /* clickSlopDp= */ 0.f,
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
                            (tooltipText) -> mToolbarControlContainer.setTooltipText(tooltipText),
                            (time, view, motionEventButtonState, modifiers) ->
                                    toggleActorTaskMenu(),
                            keyboardFocusHandler,
                            R.drawable.ic_arrow_selector_spark_16dp,
                            /* clickSlopDp= */ 0.f,
                            /* hasLongClickAction= */ false,
                            /* dismissButton= */ null);

            mGlicActorButton.setDrawY(GLIC_BUTTON_BACKGROUND_Y_OFFSET_DP);
            // Set width and opacity to 0 when hidden to prepare state for animations.
            mGlicActorButton.setWidth(0.0f);
            mGlicActorButton.setOpacity(0.0f);
            mGlicActorButton.setVisible(false);

            mGlicActorButton.setBackgroundTint(
                    backgroundDefaultColor,
                    backgroundHoverColor,
                    backgroundPressedColor,
                    backgroundPressedColor);

            mGlicActorButton.setTint(SemanticColorUtils.getDefaultIconColor(mContext));

            mGlicActorButton.setAccessibilityDescription(
                    mContext.getString(R.string.actor_task_indicator_tooltip));
        }

        updateGlicButtonOpacity(isAppInDesktopWindow, isTopResumedActivity);
    }

    /** Destroys the coordinator and unregisters observers. */
    public void destroy() {
        if (mStateController != null) {
            mStateController.destroy();
            mStateController = null;
        }
        if (mGlicKeyedService != null) {
            mGlicKeyedService.removeGlobalShowHideObserver(mGlicUiObserver);
            mGlicKeyedService.removeAllowedChangedObserver(mAllowedChangedObserver);
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

        updateGlicKeyedService(profile);

        GlicButtonStateController stateController = getOrCreateStateController();
        if (stateController != null) {
            stateController.updateObservations(profile);
        }

        onGlicPrefChanged();
        updateIsPanelOpen();
    }

    private void updateGlicKeyedService(Profile profile) {
        GlicKeyedService service = GlicKeyedServiceFactory.getForProfile(profile);
        if (mGlicKeyedService == service) return;

        if (mGlicKeyedService != null) {
            mGlicKeyedService.removeGlobalShowHideObserver(mGlicUiObserver);
            mGlicKeyedService.removeAllowedChangedObserver(mAllowedChangedObserver);
        }

        mGlicKeyedService = service;

        if (mGlicKeyedService != null) {
            mGlicKeyedService.addGlobalShowHideObserver(mGlicUiObserver);
            mGlicKeyedService.addAllowedChangedObserver(mAllowedChangedObserver);
        }
    }

    private void updateIsPanelOpen() {
        if (mProfile == null || mGlicKeyedService == null || mTaskTracker == null) return;
        Activity activity = ContextUtils.activityFromContext(mContext);
        if (activity == null) return;
        var task = mTaskTracker.get(activity.getTaskId());
        if (task == null) return;
        long browserWindowPtr = task.getNativeBrowserWindowPtr(mProfile, activity);
        boolean isOpened = false;
        if (browserWindowPtr != 0 && !activity.isDestroyed()) {
            isOpened = mGlicKeyedService.isPanelShowingForBrowser(browserWindowPtr);
        }

        if (mIsGlicUiVisible == isOpened) return;

        mIsGlicUiVisible = isOpened;
        if (mGlicButton != null) {
            mGlicButton.setAccessibilityDescription(
                    mContext.getString(
                            isOpened
                                    ? R.string.glic_tab_strip_button_tooltip_close
                                    : R.string.glic_tab_strip_button_tooltip));
        }
    }

    private void onGlicPrefChanged() {
        updateTrailingButtonsState(/* animate= */ false, /* forceLayoutChanged= */ false);
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

        updateTrailingButtonsState(/* animate= */ false, /* forceLayoutChanged= */ true);

        // Dismiss trailing buttons' menus, similar to how the app menu is dismissed on
        // orientation change
        dismissTrailingButtonsMenu();
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
        GlicButtonStateController stateController = getOrCreateStateController();
        if (stateController != null) {
            stateController.setPersistDoneState(false);
        }

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
                            mContext,
                            mTabModelSelectorSupplier,
                            mGlicClickHandler,
                            GlicInvocationSource.TOP_CHROME_BUTTON,
                            GlicTaskMenuCoordinator.ButtonSource.TAB_STRIP);
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
    /* package */ void updateButtonTextProperties(TintedCompositorTextButton button) {
        boolean isActor = button == mGlicActorButton;
        String text = button.getText();
        if (mLayerTitleCache != null && !TextUtils.isEmpty(text)) {
            button.setTextResourceId(mLayerTitleCache.getUpdatedGlicButtonText(text, isActor));
        } else {
            button.setTextResourceId(Resources.ID_NULL);
        }
        updateGlicButtonWidth(mLayerTitleCache, isActor);
        updateGlicButtonPosition();
        mObserver.onTrailingButtonsLayoutStateChanged();
    }

    private void updateGlicButtonWidth(@Nullable LayerTitleCache titleCache, boolean isActor) {
        if (mGlicButton == null || mGlicActorButton == null) return;
        if (isActor) {
            float targetWidth = calculateButtonWidth(mGlicActorButton, titleCache);
            animateGlicButton(
                    mGlicActorButton,
                    targetWidth,
                    1.0f,
                    /* isActor= */ true,
                    /* endAction= */ null);
        } else {
            float targetWidth = calculateButtonWidth(mGlicButton, titleCache);
            animateGlicButton(
                    mGlicButton, targetWidth, 1.0f, /* isActor= */ false, /* endAction= */ null);
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

    private void animateGlicButton(
            TintedCompositorTextButton button,
            float targetWidth,
            float targetOpacity,
            boolean isActor,
            @Nullable Runnable endAction) {
        CompositorAnimator widthAnimator =
                isActor ? mGlicActorButtonWidthAnimator : mGlicButtonWidthAnimator;
        if (widthAnimator != null && widthAnimator.isRunning()) {
            widthAnimator.cancel();
        }
        CompositorAnimator opacityAnimator =
                isActor ? mGlicActorButtonOpacityAnimator : mGlicButtonOpacityAnimator;
        if (opacityAnimator != null && opacityAnimator.isRunning()) {
            opacityAnimator.cancel();
        }
        FloatProperty<StripLayoutTrailingButtonsCoordinator> property =
                isActor ? GLIC_ACTOR_BUTTON_WIDTH : GLIC_BUTTON_WIDTH;

        boolean expanding = targetWidth > button.getWidth();
        int duration = expanding ? GLIC_ANIMATION_EXPANSION_IN_MS : GLIC_ANIMATION_EXPANSION_OUT_MS;
        widthAnimator =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        this,
                        property,
                        button.getWidth(),
                        targetWidth,
                        duration);
        widthAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);

        float startOpacity = button.getOpacity();
        int opacityDuration =
                expanding ? GLIC_ANIMATION_OPACITY_IN_MS : GLIC_ANIMATION_OPACITY_OUT_MS;
        int opacityDelay = expanding ? GLIC_ANIMATION_OPACITY_DELAY_MS : 0;
        opacityAnimator =
                CompositorAnimator.ofFloatProperty(
                        mUpdateHost.getAnimationHandler(),
                        button,
                        CompositorButton.OPACITY,
                        startOpacity,
                        targetOpacity,
                        opacityDuration);
        opacityAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        if (opacityDelay > 0) {
            opacityAnimator.setStartDelay(opacityDelay);
        }

        if (mGlicDismissButtonSlideAnimator != null
                && mGlicDismissButtonSlideAnimator.isRunning()) {
            mGlicDismissButtonSlideAnimator.cancel();
        }

        CompositorAnimator slideAnimator = null;
        if (!isActor && isGlicDismissNudgeButtonVisible()) {
            // When expanding to show a nudge, snap the dismiss button rightward to hide
            // off-canvas, then slide in to sync with pill expansion.
            mDismissButtonXOffset = Math.abs(targetWidth - button.getWidth());
            slideAnimator =
                    CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            this,
                            GLIC_DISMISS_BUTTON_X_OFFSET,
                            mDismissButtonXOffset,
                            0.f,
                            duration);
            slideAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
        } else {
            mDismissButtonXOffset = 0.f;
        }

        final CompositorAnimator finalWidthAnimator = widthAnimator;
        final CompositorAnimator finalOpacityAnimator = opacityAnimator;
        final CompositorAnimator finalSlideAnimator = slideAnimator;

        AnimatorListenerAdapter listener =
                new AnimatorListenerAdapter() {
                    private boolean mCanceled;

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        mCanceled = true;
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        if (mGlicActorButtonWidthAnimator == finalWidthAnimator) {
                            mGlicActorButtonWidthAnimator = null;
                        }
                        if (mGlicActorButtonOpacityAnimator == finalOpacityAnimator) {
                            mGlicActorButtonOpacityAnimator = null;
                        }
                        if (mGlicButtonWidthAnimator == finalWidthAnimator) {
                            mGlicButtonWidthAnimator = null;
                        }
                        if (mGlicButtonOpacityAnimator == finalOpacityAnimator) {
                            mGlicButtonOpacityAnimator = null;
                        }
                        if (mGlicDismissButtonSlideAnimator == finalSlideAnimator) {
                            mGlicDismissButtonSlideAnimator = null;
                        }
                        if (!mCanceled && endAction != null) {
                            endAction.run();
                        }

                        mObserver.onTrailingButtonsLayoutStateChanged();
                    }
                };

        if (isActor) {
            mGlicActorButtonWidthAnimator = widthAnimator;
            mGlicActorButtonOpacityAnimator = opacityAnimator;
        } else {
            mGlicButtonWidthAnimator = widthAnimator;
            mGlicButtonOpacityAnimator = opacityAnimator;
            mGlicDismissButtonSlideAnimator = slideAnimator;
        }

        List<Animator> animators = new ArrayList<>();
        animators.add(widthAnimator);
        animators.add(opacityAnimator);
        if (slideAnimator != null) {
            animators.add(slideAnimator);
        }

        startAnimations(animators, listener);
    }

    /**
     * Starts the given list of animators together in an AnimatorSet.
     *
     * @param animators The list of animators to start.
     * @param listener An optional listener to attach to the AnimatorSet.
     */
    @VisibleForTesting
    /* package */ void startAnimations(
            List<Animator> animators, @Nullable AnimatorListenerAdapter listener) {
        if (animators == null || animators.isEmpty()) return;

        AnimatorSet set = new AnimatorSet();
        set.playTogether(animators);
        if (listener != null) {
            set.addListener(listener);
        }
        set.start();
    }

    private void cancelRunningAnimators() {
        if (mGlicButtonWidthAnimator != null && mGlicButtonWidthAnimator.isRunning()) {
            mGlicButtonWidthAnimator.cancel();
        }
        if (mGlicActorButtonWidthAnimator != null && mGlicActorButtonWidthAnimator.isRunning()) {
            mGlicActorButtonWidthAnimator.cancel();
        }
        if (mGlicButtonOpacityAnimator != null && mGlicButtonOpacityAnimator.isRunning()) {
            mGlicButtonOpacityAnimator.cancel();
        }
        if (mGlicActorButtonOpacityAnimator != null
                && mGlicActorButtonOpacityAnimator.isRunning()) {
            mGlicActorButtonOpacityAnimator.cancel();
        }
        if (mGlicDismissButtonSlideAnimator != null
                && mGlicDismissButtonSlideAnimator.isRunning()) {
            mGlicDismissButtonSlideAnimator.cancel();
        }
    }

    private void updateTrailingButtonsState(boolean animate, boolean forceLayoutChanged) {
        if (mGlicButton == null || mGlicActorButton == null) return;

        // 1. Query target visibilities
        boolean targetGlicVisible = shouldGlicBeVisible();
        boolean targetActorVisible = shouldGlicActorBeVisible();

        // 2. Resolve target text
        String targetGlicText = null;
        String targetActorText = null;
        if (targetActorVisible) {
            // Glic button collapses its text to let the actor button take focus
            if (mWidth >= GLIC_ACTOR_TEXT_HIDE_THRESHOLD_DP) {
                targetActorText =
                        (mLastGlicActorButtonState == ButtonState.DONE)
                                ? mContext.getString(R.string.glic_button_status_done)
                                : null;
            }
        } else {
            // When actor is not visible, Glic button keeps its custom text if a nudge is
            // showing; otherwise, it defaults to the standard label.
            if (isGlicDismissNudgeButtonVisible()) {
                targetGlicText = mGlicButton.getText();
            } else {
                targetGlicText =
                        mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label);
            }
            targetActorText = null;
        }

        // 3. Apply visibility, text, and width updates
        boolean layoutChanged = forceLayoutChanged;
        boolean glicVisibilityChanged = mGlicButton.isVisible() != targetGlicVisible;
        boolean actorVisibilityChanged = mGlicActorButton.isVisible() != targetActorVisible;

        if (glicVisibilityChanged) {
            setGlicButtonVisible(targetGlicVisible);
            layoutChanged = true;
        }

        if (actorVisibilityChanged) {
            layoutChanged = true;
            setGlicActorButtonVisible(targetActorVisible, animate);
        }

        setGlicButtonText(targetGlicText, /* isActor= */ false);
        setGlicButtonText(targetActorText, /* isActor= */ true);

        // 4. Recalculate button widths and apply transitions
        float targetGlicWidth = calculateButtonWidth(mGlicButton, mLayerTitleCache);
        float targetActorWidth =
                targetActorVisible
                        ? calculateButtonWidth(mGlicActorButton, mLayerTitleCache)
                        : 0.0f;
        float currentGlicWidth = mGlicButton.getWidth();
        float currentActorWidth = mGlicActorButton.getWidth();
        if (currentGlicWidth != targetGlicWidth || currentActorWidth != targetActorWidth) {
            layoutChanged = true;
        }
        updateGlicButtonsVisualProperties(animate, targetGlicWidth, targetActorWidth);

        // 5. Reposition coordinates and notify host
        if (layoutChanged) {
            updateGlicButtonPosition();
            mObserver.onTrailingButtonsLayoutStateChanged();
            mUpdateHost.requestUpdate();
        }
    }

    private void updateGlicButtonsVisualProperties(
            boolean animate, float targetGlicWidth, float targetActorWidth) {
        if (mGlicButton == null || mGlicActorButton == null) return;

        float targetOpacity = mIsUnfocusedInDw ? GLIC_BUTTON_UNFOCUSED_OPACITY : 1.0f;
        boolean targetActorVisible = shouldGlicActorBeVisible();

        if (animate) {
            animateGlicButton(
                    mGlicButton, targetGlicWidth, targetOpacity, /* isActor= */ false, null);
            if (targetActorVisible) {
                animateGlicButton(
                        mGlicActorButton,
                        targetActorWidth,
                        targetOpacity,
                        /* isActor= */ true,
                        null);
            }
        } else {
            // 1. Cancel running animators instantly to prevent property fighting
            cancelRunningAnimators();

            // 2. Set layout properties directly
            mGlicButton.setWidth(targetGlicWidth);
            mGlicButton.setOpacity(targetOpacity);
            mGlicActorButton.setWidth(targetActorWidth);
            mGlicActorButton.setOpacity(targetActorVisible ? targetOpacity : 0.0f);
            mDismissButtonXOffset = 0.f;
        }
    }

    @VisibleForTesting
    /* package */ void setGlicButtonText(@Nullable String text, boolean isActor) {
        TintedCompositorTextButton button = isActor ? mGlicActorButton : mGlicButton;
        if (button == null) return;
        if (TextUtils.equals(button.getText(), text)) return;

        button.setText(text);

        if (mLayerTitleCache != null && !TextUtils.isEmpty(text)) {
            button.setTextResourceId(
                    mLayerTitleCache.getUpdatedGlicButtonText(
                            text, /* isActor= */ button == mGlicActorButton));
        } else {
            button.setTextResourceId(Resources.ID_NULL);
        }

        // TODO(crbug.com/518925727): Check if we need to change a11y string for Glic with nudge.
        if (button == mGlicActorButton) {
            if (TextUtils.isEmpty(text)) {
                button.setAccessibilityDescription(
                        mContext.getString(R.string.actor_task_indicator_tooltip));
            } else {
                button.setAccessibilityDescription(text);
            }
        }
    }

    /** Updates the position of the Glic buttons based on layout parameters. */
    public void updateGlicButtonPosition() {
        if (mGlicButton == null || mGlicDismissNudgeButton == null || mGlicActorButton == null) {
            return;
        }

        // 1. X Positions
        if (!LocalizationUtils.isLayoutRtl()) {
            float rightSideAnchor = mWidth - mRightPadding;
            if (mIsMsbVisible) {
                rightSideAnchor -= StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE;
            }
            rightSideAnchor -= GLIC_BUTTON_END_SLOP_DP;
            if (isGlicActorButtonVisible()) {
                mGlicActorButton.setDrawX(rightSideAnchor - mGlicActorButton.getWidth());
                rightSideAnchor -= mGlicActorButton.getWidth() + GLIC_ACTOR_BUTTON_GAP_DP;
            }
            mGlicButton.setDrawX(rightSideAnchor - mGlicButton.getWidth());
            if (mGlicDismissNudgeButton.isVisible()) {
                mGlicDismissNudgeButton.setDrawX(
                        rightSideAnchor
                                - GLIC_BUTTON_SHORTENED_END_PADDING_DP
                                - GLIC_DISMISS_ICON_WIDTH_DP
                                + mDismissButtonXOffset);
            }
        } else {
            float leftSideAnchor = mLeftPadding;
            if (mIsMsbVisible) {
                leftSideAnchor += StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE;
            }
            leftSideAnchor += GLIC_BUTTON_END_SLOP_DP;
            if (isGlicActorButtonVisible()) {
                mGlicActorButton.setDrawX(leftSideAnchor);
                leftSideAnchor += mGlicActorButton.getWidth() + GLIC_ACTOR_BUTTON_GAP_DP;
            }
            mGlicButton.setDrawX(leftSideAnchor);
            if (mGlicDismissNudgeButton.isVisible()) {
                mGlicDismissNudgeButton.setDrawX(
                        leftSideAnchor
                                + GLIC_BUTTON_SHORTENED_END_PADDING_DP
                                - mDismissButtonXOffset);
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
        float endSlop =
                isGlicActorButtonVisible()
                        ? GLIC_BUTTON_WITH_ACTOR_END_SLOP_DP
                        : GLIC_BUTTON_END_SLOP_DP;
        mGlicButton.setTouchTargetInsets(
                -GLIC_BUTTON_START_SLOP_DP,
                -GLIC_BUTTON_VERTICAL_SLOP_DP + mTopPadding,
                -endSlop,
                -GLIC_BUTTON_VERTICAL_SLOP_DP - mTopPadding);
        mGlicActorButton.setTouchTargetInsets(
                -GLIC_ACTOR_START_SLOP_DP,
                -GLIC_BUTTON_VERTICAL_SLOP_DP + mTopPadding,
                -GLIC_BUTTON_END_SLOP_DP,
                -GLIC_BUTTON_VERTICAL_SLOP_DP - mTopPadding);
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
        mIsUnfocusedInDw = isAppInDesktopWindow && !isTopResumedActivity;
        float targetOpacity = mIsUnfocusedInDw ? GLIC_BUTTON_UNFOCUSED_OPACITY : 1.0f;
        mGlicButton.setOpacity(targetOpacity);
        mGlicActorButton.setOpacity(targetOpacity);
    }

    /** Returns the total width used by the trailing buttons including padding. */
    public float getTrailingButtonsWidthWithPadding() {
        float width = 0.0f;
        if (isGlicButtonVisible()) {
            width += mGlicButton.getWidth() + GLIC_BUTTON_START_SLOP_DP + GLIC_BUTTON_END_SLOP_DP;
        }
        if (isGlicActorButtonVisible()) {
            // Add spacing gap regardless of whether primary Glic button is showing.
            width += GLIC_ACTOR_BUTTON_GAP_DP;
            width += mGlicActorButton.getWidth();
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

    @VisibleForTesting
    /* package */ void setGlicDismissNudgeButtonVisible(boolean isVisible) {
        if (mGlicDismissNudgeButton == null || mGlicDismissNudgeButton.isVisible() == isVisible) {
            return;
        }

        mGlicDismissNudgeButton.setVisible(isVisible);
        updateTrailingButtonsState(/* animate= */ true, /* forceLayoutChanged= */ false);
    }

    /** Returns whether the Glic dismiss nudge button is currently visible. */
    @EnsuresNonNullIf("mGlicDismissNudgeButton")
    public boolean isGlicDismissNudgeButtonVisible() {
        return mGlicDismissNudgeButton != null && mGlicDismissNudgeButton.isVisible();
    }

    @VisibleForTesting
    /* package */ void setGlicActorButtonVisible(boolean visible, boolean animate) {
        if (mGlicActorButton == null || mGlicActorButton.isVisible() == visible) return;

        if (visible) {
            mGlicActorButton.setVisible(true);
        } else {
            if (animate) {
                setGlicButtonText(null, /* isActor= */ true);
                animateGlicButton(
                        mGlicActorButton,
                        0.0f,
                        0.0f,
                        /* isActor= */ true,
                        () -> {
                            if (mGlicActorButton != null) {
                                mGlicActorButton.setVisible(false);
                            }
                            updateGlicButtonPosition();
                            mUpdateHost.requestUpdate();
                        });
            } else {
                mGlicActorButton.setOpacity(0.0f);
                mGlicActorButton.setWidth(0.0f);
                mGlicActorButton.setVisible(false);
            }
        }
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
        return GlicEnabling.isEnabledForProfile(mProfile)
                && GlicUtils.isButtonPinnedToTabStrip(mProfile);
    }

    /**
     * Determines whether the Glic actor button should be visible in the tab strip.
     *
     * @return true if the Glic actor button should be visible.
     */
    public boolean shouldGlicActorBeVisible() {
        GlicButtonStateController stateController = getOrCreateStateController();
        if (!shouldGlicBeVisible() || mGlicActorButton == null || stateController == null) {
            return false;
        }

        // TODO(crbug.com/507213867): Change to check for all tasks (active, recently finished).
        if (stateController.getButtonState() == ButtonState.DONE) {
            return true;
        }
        List<ActorTask> tasks = stateController.getActiveTasks();
        return tasks != null && !tasks.isEmpty();
    }

    @VisibleForTesting
    /* package */ void onGlicActorButtonStateChanged(@ButtonState int state, boolean isPanelOpen) {
        if (mStateController == null || mGlicActorButton == null || mGlicButton == null) return;
        if (mLastGlicActorButtonState == state) return;
        mLastGlicActorButtonState = state;

        updateTrailingButtonsState(/* animate= */ true, /* forceLayoutChanged= */ false);
    }

    private @Nullable GlicButtonStateController getOrCreateStateController() {
        if (mStateController != null) return mStateController;

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null || mIsIncognitoSupplier.get()) return null;

        mStateController =
                new GlicButtonStateController(
                        activity,
                        this::onGlicActorButtonStateChanged,
                        () -> mTaskTracker != null ? mTaskTracker.get(activity.getTaskId()) : null,
                        /* browserControlsVisibilityManager= */ null);
        if (mProfile != null) {
            mStateController.updateObservations(mProfile);
        }
        return mStateController;
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
            mGlicClickHandler.onClick(
                    /* preventClose= */ false, GlicInvocationSource.TOP_CHROME_BUTTON);
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
            if (MotionEventUtils.isSecondaryClick(buttons)) {
                // Consume secondary click to prevent triggering empty space context menu.
                return true;
            } else if (mGlicActorButton.click(x, y, buttons)) {
                mGlicActorButton.handleClick(time, buttons, modifiers);
                return true;
            }
        }
        return false;
    }
}
