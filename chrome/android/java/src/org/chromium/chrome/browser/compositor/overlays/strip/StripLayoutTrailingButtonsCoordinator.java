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
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.TabStripLayoutType;
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.glic.GlicButtonStateController;
import org.chromium.chrome.browser.glic.GlicButtonStateController.ButtonState;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicHelper;
import org.chromium.chrome.browser.glic.GlicKeyedService;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlobalShowHideObserver;
import org.chromium.chrome.browser.glic.GlicKeyedServiceFactory;
import org.chromium.chrome.browser.glic.GlicNudgeActivity;
import org.chromium.chrome.browser.glic.GlicNudgeDelegate;
import org.chromium.chrome.browser.glic.GlicNudgeDelegateBridge;
import org.chromium.chrome.browser.glic.GlicPrefNames;
import org.chromium.chrome.browser.glic.GlicTaskMenuCoordinator;
import org.chromium.chrome.browser.glic.GlicUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeatureKey;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTracker;
import org.chromium.chrome.browser.ui.side_panel.AndroidSidePanelEnabledFn;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiShowability;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.side_ui.SideUiStateProvider;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.util.StyleUtils;
import org.chromium.ui.widget.RectProvider;

import java.util.ArrayList;
import java.util.List;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Coordinator for the trailing buttons on the tab strip. */
@NullMarked
public class StripLayoutTrailingButtonsCoordinator {
    /** Observer for changes to the trailing buttons layout state. */
    public interface StripLayoutTrailingButtonsObserver {
        /** Called when the trailing buttons layout footprint (e.g. width or visibility) changes. */
        void onTrailingButtonsLayoutStateChanged();
    }

    // Minimum strip width in DP required to show text on the Glic Actor button (below this, only
    // the icon is shown).
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
    private final ActivityWindowAndroid mWindowAndroid;

    // Configuration & Delegates
    private final float mDensity;
    private final GlicButtonDelegate mGlicClickHandler;
    private final GlobalShowHideObserver mGlicUiObserver;
    private final GlicKeyedService.AllowedChangedObserver mAllowedChangedObserver =
            () -> updateTrailingButtonsState(/* animate= */ false, /* forceLayoutChanged= */ false);
    private final ChromeAndroidTaskTracker mTaskTracker;
    private boolean mIsIncognito;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private final OneshotSupplier<SideUiStateProvider> mSideUiStateProviderSupplier;
    private final Supplier<Float> mTabWidthSupplier;
    private final BooleanSupplier mGlicIphShowingSupplier;
    private final StripLayoutTrailingButtonsObserver mObserver;
    private @Nullable SideUiStateProvider mSideUiStateProvider;
    private final SideUiObserver mSideUiObserver =
            new SideUiObserver() {
                @Override
                public void onSideUiSpecsChanged(SideUiSpecs sideUiSpecs) {}

                @Override
                public void onShowableSideUisUpdated(SideUiShowability sideUiShowability) {
                    if (sideUiShowability.mShowableSideUiIds.contains(SideUiId.SIDE_PANEL)
                            || sideUiShowability.mUnshowableSideUiIds.contains(
                                    SideUiId.SIDE_PANEL)) {
                        updateTrailingButtonsState(
                                /* animate= */ false, /* forceLayoutChanged= */ false);
                    }
                }
            };

    // Lifecycle & Caching Objects
    private @Nullable Profile mProfile;
    private @Nullable PrefChangeRegistrar mPrefChangeRegistrar;
    private @Nullable LayerTitleCache mLayerTitleCache;
    private @Nullable GlicKeyedService mGlicKeyedService;

    // Callbacks
    private final Runnable mModelSelectorButtonClickHandler;
    private final StripLayoutViewOnKeyboardFocusHandler mModelSelectorButtonKeyboardFocusHandler;

    // UI Components
    private @Nullable TintedCompositorButton mModelSelectorButton;
    private @Nullable TintedCompositorTextButton mGlicButton;
    private @Nullable TintedCompositorButton mGlicDismissNudgeButton;
    private @Nullable TintedCompositorTextButton mGlicActorButton;
    private @Nullable GlicButtonContextMenuCoordinator mGlicButtonContextMenuCoordinator;
    private @Nullable GlicTaskMenuCoordinator mGlicTaskMenuCoordinator;
    private @Nullable GlicButtonStateController mStateController;
    private final View mToolbarControlContainer;

    private final GlicNudgeDelegate mGlicNudgeDelegate =
            new GlicNudgeDelegate() {
                @Override
                public void onTriggerGlicNudgeUi(
                        String label, String anchoredMessageText, String promptSuggestion) {
                    if (mGlicIphShowingSupplier.getAsBoolean()) {
                        mGlicNudgeDelegateBridge.onNudgeActivity(
                                GlicNudgeActivity.NUDGE_NOT_SHOWN_WINDOW_CALL_TO_ACTION_UI);
                        return;
                    }
                    mNudgeLabel = label;
                    updateTrailingButtonsState(
                            /* animate= */ true, /* forceLayoutChanged= */ false);
                }

                @Override
                public void onHideGlicNudgeUi() {
                    mNudgeLabel = null;
                    updateTrailingButtonsState(
                            /* animate= */ true, /* forceLayoutChanged= */ false);
                }

                @Override
                public boolean getIsShowingGlicNudge() {
                    return mNudgeLabel != null;
                }
            };
    private final GlicNudgeDelegateBridge mGlicNudgeDelegateBridge =
            new GlicNudgeDelegateBridge(mGlicNudgeDelegate);

    // Layout & State Parameters
    private float mWidth;
    private float mRightPadding;
    private float mLeftPadding;
    private float mTopPadding;
    private boolean mIsGlicUiVisible;
    private int mLastGlicActorButtonState = ButtonState.DEFAULT;
    private boolean mIsTopResumedActivity;
    private boolean mIsAppInDesktopWindow;
    private @Nullable String mNudgeLabel;

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
                        object.updateButtonPositions();
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
                                object.updateButtonPositions();
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
                            object.updateButtonPositions();
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
     * @param density The display density.
     * @param toolbarControlContainer The view containing toolbar controls.
     * @param isAppInDesktopWindow Whether the app is in a desktop window.
     * @param isTopResumedActivity Whether the app is the top resumed activity.
     * @param taskTracker The {@link ChromeAndroidTaskTracker} for tracking tasks.
     * @param isIncognito Whether the current tab model is incognito.
     * @param tabModelSelectorSupplier Supplier for the {@link TabModelSelector}.
     * @param sideUiStateProviderSupplier Supplier for the {@link SideUiStateProvider}.
     * @param tabWidthSupplier Supplier for the unpinned tab width in DP.
     * @param modelSelectorClickHandler The click handler {@link Runnable} for the model selector
     *     button.
     * @param modelSelectorKeyboardFocusHandler The {@link StripLayoutViewOnKeyboardFocusHandler}
     *     for the model selector button.
     * @param glicClickHandler The {@link GlicButtonDelegate} to execute on Glic button click.
     * @param glicKeyboardFocusHandler The {@link StripLayoutViewOnKeyboardFocusHandler} for the
     *     Glic button.
     * @param glicIphShowingSupplier The supplier returning whether the tab strip Glic IPH is
     *     showing.
     * @param observer The {@link StripLayoutTrailingButtonsObserver} for layout state changes.
     */
    public StripLayoutTrailingButtonsCoordinator(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            ActivityWindowAndroid windowAndroid,
            float density,
            View toolbarControlContainer,
            boolean isAppInDesktopWindow,
            boolean isTopResumedActivity,
            ChromeAndroidTaskTracker taskTracker,
            boolean isIncognito,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<SideUiStateProvider> sideUiStateProviderSupplier,
            Supplier<Float> tabWidthSupplier,
            Runnable modelSelectorClickHandler,
            StripLayoutViewOnKeyboardFocusHandler modelSelectorKeyboardFocusHandler,
            GlicButtonDelegate glicClickHandler,
            StripLayoutViewOnKeyboardFocusHandler glicKeyboardFocusHandler,
            BooleanSupplier glicIphShowingSupplier,
            StripLayoutTrailingButtonsObserver observer) {
        mContext = context;
        mUpdateHost = updateHost;
        mRenderHost = renderHost;
        mDensity = density;
        mTaskTracker = taskTracker;
        mIsIncognito = isIncognito;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mSideUiStateProviderSupplier = sideUiStateProviderSupplier;
        mTabWidthSupplier = tabWidthSupplier;
        mModelSelectorButtonClickHandler = modelSelectorClickHandler;
        mModelSelectorButtonKeyboardFocusHandler = modelSelectorKeyboardFocusHandler;
        mGlicClickHandler = glicClickHandler;
        mGlicIphShowingSupplier = glicIphShowingSupplier;
        mObserver = observer;
        mWindowAndroid = windowAndroid;
        mToolbarControlContainer = toolbarControlContainer;
        mGlicUiObserver = this::updateIsPanelOpen;

        if (!IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            float bgSizeDp = getDimensionDp(R.dimen.tab_strip_button_bg_size);
            mModelSelectorButton =
                    new TintedCompositorButton(
                            mContext,
                            mIsIncognito,
                            ButtonType.INCOGNITO_SWITCHER,
                            /* parentView= */ null,
                            bgSizeDp,
                            bgSizeDp,
                            (tooltipText) -> {
                                mToolbarControlContainer.setTooltipText(tooltipText);
                            },
                            (time, view, motionEventButtonState, modifiers) -> {
                                mModelSelectorButtonClickHandler.run();
                            },
                            mModelSelectorButtonKeyboardFocusHandler,
                            R.drawable.ic_incognito,
                            R.drawable.bg_circle_tab_strip_button,
                            getModelSelectorButtonClickSlopDp());

            mModelSelectorButton.setDrawY(getDimensionDp(R.dimen.tab_strip_button_y_offset));
            updateModelSelectorButtonProperties();
            mModelSelectorButton.setVisible(false);
        }

        if (GlicEnabling.isEnabledByFlags() && AndroidSidePanelEnabledFn.isEnabled()) {
            mSideUiStateProviderSupplier.onAvailable(
                    (provider) -> {
                        mSideUiStateProvider = provider;
                        mSideUiStateProvider.addObserver(mSideUiObserver);
                        updateTrailingButtonsState(
                                /* animate= */ false, /* forceLayoutChanged= */ false);
                    });

            StripLayoutViewOnClickHandler glicClickHandlerOnButton =
                    (time, view, motionEventButtonState, modifiers) ->
                            handleGlicButtonClick(/* preventClose= */ false);

            float dismissIconWidthDp = getDimensionDp(R.dimen.tab_strip_glic_dismiss_icon_width);
            mGlicDismissNudgeButton =
                    new TintedCompositorButton(
                            mContext,
                            /* incognito= */ false,
                            ButtonType.GLIC_DISMISS_NUDGE,
                            /* parentView= */ null,
                            dismissIconWidthDp,
                            dismissIconWidthDp,
                            (tooltipText) -> mToolbarControlContainer.setTooltipText(tooltipText),
                            (time, view, motionEventButtonState, modifiers) -> {
                                handleDismissButtonClick();
                            },
                            glicKeyboardFocusHandler,
                            R.drawable.btn_tab_close_normal,
                            Resources.ID_NULL,
                            /* clickSlopDp= */ 0.f,
                            /* hasLongClickAction= */ false);

            mGlicDismissNudgeButton.setDrawY(
                    getDimensionDp(R.dimen.tab_strip_glic_dismiss_button_y_offset));
            mGlicDismissNudgeButton.setVisible(false);
            mGlicDismissNudgeButton.setAccessibilityDescription(
                    mContext.getString(R.string.tooltip_glic_close));
            @ColorInt
            int dismissIconDefaultColor = SemanticColorUtils.getDefaultIconColor(mContext);
            mGlicDismissNudgeButton.setTint(dismissIconDefaultColor);

            float bgWidthDp = getDimensionDp(R.dimen.tab_strip_glic_button_bg_width);
            float bgHeightDp = getDimensionDp(R.dimen.tab_strip_button_bg_size);
            mGlicButton =
                    new TintedCompositorTextButton(
                            mContext,
                            /* incognito= */ false,
                            ButtonType.GLIC,
                            /* parentView= */ null,
                            bgWidthDp,
                            bgHeightDp,
                            (tooltipText) -> mToolbarControlContainer.setTooltipText(tooltipText),
                            glicClickHandlerOnButton,
                            glicKeyboardFocusHandler,
                            R.drawable.ic_spark_4c_16dp,
                            /* clickSlopDp= */ 0.f,
                            /* hasLongClickAction= */ true,
                            mGlicDismissNudgeButton);

            mGlicButton.setOnLongClickHandler(
                    view -> {
                        Activity activity = mWindowAndroid.getActivity().get();
                        if (activity != null) {
                            showMenu(activity);
                            // Clear the pressed state so a click isn't triggered in addition to the
                            // long press.
                            if (mGlicButton != null) {
                                mGlicButton.setPressed(false);
                            }
                        }
                    });

            mGlicButtonContextMenuCoordinator =
                    new GlicButtonContextMenuCoordinator(mContext, TabStripLayoutType.HORIZONTAL);

            mGlicButton.setDrawY(getDimensionDp(R.dimen.tab_strip_button_y_offset));
            mGlicButton.setVisible(false);

            mGlicButton.setText(
                    mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label));
            updateGlicButtonAccessibilityDescription();

            mGlicActorButton =
                    new TintedCompositorTextButton(
                            mContext,
                            /* incognito= */ false,
                            ButtonType.GLIC_ACTOR,
                            /* parentView= */ null,
                            bgWidthDp,
                            bgHeightDp,
                            (tooltipText) -> mToolbarControlContainer.setTooltipText(tooltipText),
                            (time, view, motionEventButtonState, modifiers) ->
                                    handleGlicActorButtonClick(),
                            glicKeyboardFocusHandler,
                            R.drawable.ic_arrow_selector_spark_16dp,
                            /* clickSlopDp= */ 0.f,
                            /* hasLongClickAction= */ false,
                            /* dismissButton= */ null);

            mGlicActorButton.setDrawY(getDimensionDp(R.dimen.tab_strip_button_y_offset));
            // Set width and opacity to 0 when hidden to prepare state for animations.
            mGlicActorButton.setWidth(0.0f);
            mGlicActorButton.setOpacity(0.0f);
            mGlicActorButton.setVisible(false);
            mGlicActorButton.setBackgroundTint(
                    mContext.getColorStateList(R.color.tab_strip_glic_button_bg_tint_list));

            mGlicActorButton.setTint(SemanticColorUtils.getDefaultIconColor(mContext));

            mGlicActorButton.setAccessibilityDescription(
                    mContext.getString(R.string.actor_task_indicator_tooltip));
        }

        updateButtonTints(mIsIncognito);
        updateGlicButtonOpacity(isAppInDesktopWindow, isTopResumedActivity);
    }

    /** Destroys the coordinator and unregisters observers. */
    public void destroy() {
        if (mSideUiStateProvider != null) {
            mSideUiStateProvider.removeObserver(mSideUiObserver);
            mSideUiStateProvider = null;
        }
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
        mModelSelectorButton = null;
    }

    /**
     * Registers a pref observer for Glic button changes when the profile is available, and
     * creates/recreates the nudge delegate bridge.
     *
     * @param profile The {@link Profile} to observe.
     */
    public void onProfileAvailable(Profile profile) {
        if (mProfile == profile) return;
        mProfile = profile;

        Activity activity = mWindowAndroid.getActivity().get();
        if (activity != null) {
            ChromeAndroidTask task = mTaskTracker.get(activity.getTaskId());
            if (task != null) {
                task.addFeature(
                        new ChromeAndroidTaskFeatureKey(
                                GlicNudgeDelegateBridge.class, profile, mWindowAndroid),
                        () -> mGlicNudgeDelegateBridge);
            }
        }

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
        if (mProfile == null || mGlicKeyedService == null) return;
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
        updateGlicButtonAccessibilityDescription();
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

        if (mGlicButton != null || mModelSelectorButton != null) {
            updateTrailingButtonsState(/* animate= */ false, /* forceLayoutChanged= */ true);
        }

        // Dismiss trailing buttons' menus, similar to how the app menu is dismissed on
        // orientation change
        dismissTrailingButtonsMenu();
    }

    /**
     * Called when the active tab model switches. Updates Glic button tints and resets its
     * text/nudge state if switching to incognito mode.
     *
     * @param incognito Whether the new tab model is incognito.
     */
    public void onTabModelSwitched(boolean incognito) {
        mIsIncognito = incognito;
        updateButtonTints(incognito);

        if (mGlicButton != null || mModelSelectorButton != null) {
            updateTrailingButtonsState(/* animate= */ false, /* forceLayoutChanged= */ true);
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

    private void handleGlicActorButtonClick() {
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
        if (tasks.isEmpty()) {
            handleGlicButtonClick(/* preventClose= */ true);
            return;
        }

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
     */
    public void showMenu(Activity activity) {
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
                anchorRectProvider, activity, mProfile, mTabWidthSupplier.get());
    }

    /**
     * Opens the context menu for the currently keyboard-focused trailing button, if applicable.
     *
     * @param activity The current {@link Activity}.
     * @return Whether the context menu was successfully opened.
     */
    public boolean openKeyboardFocusedContextMenu(Activity activity) {
        if (isGlicButtonVisible() && mGlicButton.isKeyboardFocused()) {
            showMenu(activity);
            return true;
        }
        return false;
    }

    private void updateButtonTints(boolean incognito) {
        if (mGlicButton == null) return;

        @ColorInt
        int iconTint =
                incognito
                        ? mContext.getColor(R.color.tab_strip_glic_icon_incognito_tint_list)
                        : SemanticColorUtils.getDefaultIconColor(mContext);
        @ColorRes
        int bgTintRes =
                incognito
                        ? R.color.tab_strip_glic_button_bg_incognito_tint_list
                        : R.color.tab_strip_glic_button_bg_tint_list;
        mGlicButton.setTint(iconTint);
        mGlicButton.setBackgroundTint(mContext.getColorStateList(bgTintRes));
    }

    @VisibleForTesting
    /* package */ void updateButtonTextProperties(TintedCompositorTextButton button) {
        boolean isActor = button.getType() == ButtonType.GLIC_ACTOR;
        String text = button.getText();
        if (mLayerTitleCache != null && !TextUtils.isEmpty(text)) {
            button.setTextResourceId(
                    mLayerTitleCache.getUpdatedGlicButtonText(text, isActor, mIsIncognito));
        } else {
            button.setTextResourceId(Resources.ID_NULL);
        }
        updateGlicButtonWidth(button, mLayerTitleCache);
        updateButtonPositions();
        mObserver.onTrailingButtonsLayoutStateChanged();
    }

    private void updateGlicButtonWidth(
            TintedCompositorTextButton button, @Nullable LayerTitleCache titleCache) {
        float targetWidth = calculateGlicButtonWidth(button, titleCache);
        animateGlicButton(button, targetWidth, 1.0f, /* endAction= */ null);
    }

    private float calculateGlicButtonWidth(
            TintedCompositorTextButton button, @Nullable LayerTitleCache titleCache) {
        String text = button.getText();
        float width = getDimensionDp(R.dimen.tab_strip_glic_button_bg_width);

        if (!TextUtils.isEmpty(text) && titleCache != null) {
            width =
                    getDimensionDp(R.dimen.tab_strip_glic_button_start_padding)
                            + getDimensionDp(R.dimen.tab_strip_glic_icon_width)
                            + getDimensionDp(R.dimen.tab_strip_glic_icon_text_padding)
                            + (titleCache.getButtonTextWidth(text) / mDensity);

            if (isGlicDismissNudgeButtonVisible() && button.getType() == ButtonType.GLIC) {
                width +=
                        getDimensionDp(R.dimen.tab_strip_glic_button_shortened_end_padding)
                                + getDimensionDp(R.dimen.tab_strip_glic_dismiss_icon_width);
            } else {
                width += getDimensionDp(R.dimen.tab_strip_glic_button_standard_end_padding);
            }
        }

        return width;
    }

    private void animateGlicButton(
            TintedCompositorTextButton button,
            float targetWidth,
            float targetOpacity,
            @Nullable Runnable endAction) {
        boolean isActor = button.getType() == ButtonType.GLIC_ACTOR;
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

    /** Updates the visibility and properties of the trailing buttons. */
    public void updateTrailingButtons() {
        updateTrailingButtonsState(/* animate= */ false, /* forceLayoutChanged= */ false);
    }

    private void updateTrailingButtonsState(boolean animate, boolean forceLayoutChanged) {
        boolean layoutChanged = forceLayoutChanged;

        // 1. Query target visibilities
        if (mModelSelectorButton != null) {
            boolean targetMsbVisible = shouldModelSelectorButtonBeVisible();
            if (isModelSelectorButtonVisible() != targetMsbVisible) {
                layoutChanged = true;
                mModelSelectorButton.setVisible(targetMsbVisible);
            }
            updateModelSelectorButtonProperties();
        }

        if (mGlicButton != null && mGlicActorButton != null) {
            boolean targetGlicVisible = shouldGlicBeVisible();
            boolean targetDismissVisible = shouldGlicDismissNudgeBeVisible();
            boolean targetActorVisible = shouldGlicActorBeVisible();

            // 2. Resolve target text
            String targetGlicText = null;
            String targetActorText = null;
            if (targetActorVisible) {
                // Glic button collapses its text to let the actor button take focus
                if (mWidth >= GLIC_ACTOR_TEXT_HIDE_THRESHOLD_DP) {
                    targetActorText =
                            (mLastGlicActorButtonState == ButtonState.DONE)
                                    ? mContext.getResources()
                                            .getQuantityString(
                                                    R.plurals.actor_task_nudge_task_complete_label,
                                                    1)
                                    : null;
                }
            } else {
                // When actor is not visible, Glic button keeps its custom text if a nudge is
                // showing; otherwise, it defaults to the standard label.
                if (targetDismissVisible) {
                    targetGlicText = mNudgeLabel;
                } else {
                    targetGlicText =
                            mContext.getString(R.string.glic_button_entrypoint_ask_gemini_label);
                }
                targetActorText = null;
            }

            // 3. Apply visibility, tint, text, and width updates
            boolean glicVisibilityChanged = isGlicButtonVisible() != targetGlicVisible;
            boolean dismissVisibilityChanged =
                    isGlicDismissNudgeButtonVisible() != targetDismissVisible;
            boolean actorVisibilityChanged = isGlicActorButtonVisible() != targetActorVisible;

            if (glicVisibilityChanged) {
                layoutChanged = true;
                setGlicButtonVisible(targetGlicVisible);
            }
            if (dismissVisibilityChanged) {
                layoutChanged = true;
                setGlicDismissNudgeButtonVisible(targetDismissVisible);
            }
            if (actorVisibilityChanged) {
                layoutChanged = true;
                setGlicActorButtonVisible(targetActorVisible, animate);
            }

            setGlicButtonText(targetGlicText, forceLayoutChanged);
            setGlicActorButtonText(targetActorText, forceLayoutChanged);

            // 4. Recalculate button widths and apply transitions
            float targetGlicWidth = calculateGlicButtonWidth(mGlicButton, mLayerTitleCache);
            float targetActorWidth =
                    targetActorVisible
                            ? calculateGlicButtonWidth(mGlicActorButton, mLayerTitleCache)
                            : 0.0f;
            float currentGlicWidth = mGlicButton.getWidth();
            float currentActorWidth = mGlicActorButton.getWidth();
            if (currentGlicWidth != targetGlicWidth || currentActorWidth != targetActorWidth) {
                layoutChanged = true;
            }
            updateGlicButtonsVisualProperties(animate, targetGlicWidth, targetActorWidth);
        }

        // 5. Reposition coordinates and notify host
        if (layoutChanged) {
            updateButtonPositions();
            mObserver.onTrailingButtonsLayoutStateChanged();
            mUpdateHost.requestUpdate();
        }
    }

    private void updateGlicButtonsVisualProperties(
            boolean animate, float targetGlicWidth, float targetActorWidth) {
        if (mGlicButton == null || mGlicActorButton == null) return;

        float targetOpacity =
                isUnfocusedInDw()
                        ? mContext.getResources()
                                .getFloat(R.dimen.tab_strip_glic_button_icon_unfocused_alpha)
                        : 1.0f;
        mGlicButton.setClickableOpacityThreshold(targetOpacity);
        mGlicActorButton.setClickableOpacityThreshold(targetOpacity);
        boolean targetActorVisible = shouldGlicActorBeVisible();

        if (animate) {
            animateGlicButton(mGlicButton, targetGlicWidth, targetOpacity, null);
            if (targetActorVisible) {
                animateGlicButton(mGlicActorButton, targetActorWidth, targetOpacity, null);
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
    /* package */ void setGlicButtonText(@Nullable String text, boolean forceUpdate) {
        if (mGlicButton == null) return;
        if (TextUtils.equals(mGlicButton.getText(), text) && !forceUpdate) return;

        mGlicButton.setText(text);

        if (mLayerTitleCache != null && !TextUtils.isEmpty(text)) {
            mGlicButton.setTextResourceId(
                    mLayerTitleCache.getUpdatedGlicButtonText(
                            text, /* isActor= */ false, mIsIncognito));
        } else {
            mGlicButton.setTextResourceId(Resources.ID_NULL);
        }

        updateGlicButtonAccessibilityDescription();
    }

    @VisibleForTesting
    /* package */ void setGlicActorButtonText(@Nullable String text, boolean forceUpdate) {
        if (mGlicActorButton == null) return;
        if (TextUtils.equals(mGlicActorButton.getText(), text) && !forceUpdate) return;

        mGlicActorButton.setText(text);

        if (mLayerTitleCache != null && !TextUtils.isEmpty(text)) {
            mGlicActorButton.setTextResourceId(
                    mLayerTitleCache.getUpdatedGlicButtonText(
                            text, /* isActor= */ true, mIsIncognito));
        } else {
            mGlicActorButton.setTextResourceId(Resources.ID_NULL);
        }

        updateGlicActorButtonAccessibilityDescription();
    }

    private void updateGlicButtonAccessibilityDescription() {
        if (mGlicButton == null) return;
        mGlicButton.setEnabled(!mIsIncognito);
        if (mIsGlicUiVisible) {
            mGlicButton.setAccessibilityDescription(
                    mContext.getString(R.string.glic_tab_strip_button_tooltip_close));
            // If no tooltip is set, tooltip defaults to a11y description
            mGlicButton.setTooltipText(null);
        } else {
            String defaultTooltip = mContext.getString(R.string.glic_tab_strip_button_tooltip);
            String buttonText = mGlicButton.getText();
            String desc = TextUtils.isEmpty(buttonText) ? defaultTooltip : buttonText;
            mGlicButton.setAccessibilityDescription(desc);
            mGlicButton.setTooltipText(defaultTooltip);
        }
    }

    private void updateGlicActorButtonAccessibilityDescription() {
        if (mGlicActorButton == null) return;
        String text = mGlicActorButton.getText();
        String desc =
                TextUtils.isEmpty(text)
                        ? mContext.getString(R.string.actor_task_indicator_tooltip)
                        : text;
        mGlicActorButton.setAccessibilityDescription(desc);
    }

    /** Updates the position of the trailing buttons based on layout parameters. */
    public void updateButtonPositions() {
        float stripEndPadding =
                mContext.getResources().getDimension(R.dimen.button_end_padding) / mDensity;

        // 1. X Positions
        if (!LocalizationUtils.isLayoutRtl()) {
            float rightSideAnchor = mWidth - mRightPadding;
            if (isGlicButtonVisible()) {
                rightSideAnchor -= getGlicButtonEndOffset();
                if (isGlicActorButtonVisible()) {
                    mGlicActorButton.setDrawX(rightSideAnchor - mGlicActorButton.getWidth());
                    rightSideAnchor -=
                            mGlicActorButton.getWidth()
                                    + getDimensionDp(R.dimen.tab_strip_glic_actor_button_gap);
                }
                if (isGlicDismissNudgeButtonVisible()) {
                    mGlicDismissNudgeButton.setDrawX(
                            rightSideAnchor
                                    - getDimensionDp(
                                            R.dimen.tab_strip_glic_button_shortened_end_padding)
                                    - getDimensionDp(R.dimen.tab_strip_glic_dismiss_icon_width)
                                    + mDismissButtonXOffset);
                }
                mGlicButton.setDrawX(rightSideAnchor - mGlicButton.getWidth());
                rightSideAnchor -= mGlicButton.getWidth() + GLIC_BUTTON_START_SLOP_DP;
            }
            // TODO(crbug.com/482159010): Realign MSB with toolbar buttons
            if (isModelSelectorButtonVisible()) {
                mModelSelectorButton.setDrawX(
                        rightSideAnchor - stripEndPadding - mModelSelectorButton.getWidth());
            }

        } else {
            float leftSideAnchor = mLeftPadding;
            if (isGlicButtonVisible()) {
                leftSideAnchor += getGlicButtonEndOffset();
                if (isGlicActorButtonVisible()) {
                    mGlicActorButton.setDrawX(leftSideAnchor);
                    leftSideAnchor +=
                            mGlicActorButton.getWidth()
                                    + getDimensionDp(R.dimen.tab_strip_glic_actor_button_gap);
                }
                if (isGlicDismissNudgeButtonVisible()) {
                    mGlicDismissNudgeButton.setDrawX(
                            leftSideAnchor
                                    + getDimensionDp(
                                            R.dimen.tab_strip_glic_button_shortened_end_padding)
                                    - mDismissButtonXOffset);
                }
                mGlicButton.setDrawX(leftSideAnchor);
                leftSideAnchor += mGlicButton.getWidth() + GLIC_BUTTON_START_SLOP_DP;
            }

            if (isModelSelectorButtonVisible()) {
                mModelSelectorButton.setDrawX(leftSideAnchor + stripEndPadding);
            }
        }

        // 2. Y Positions
        if (mModelSelectorButton != null) {
            mModelSelectorButton.setDrawY(getDimensionDp(R.dimen.tab_strip_button_y_offset));
        }
        if (mGlicButton != null && mGlicDismissNudgeButton != null && mGlicActorButton != null) {
            mGlicButton.setDrawY(getDimensionDp(R.dimen.tab_strip_button_y_offset));
            mGlicDismissNudgeButton.setDrawY(
                    getDimensionDp(R.dimen.tab_strip_glic_dismiss_button_y_offset));
            mGlicActorButton.setDrawY(getDimensionDp(R.dimen.tab_strip_button_y_offset));
        }

        // 3. Touch Targets
        updateTouchTargetInsets();
    }

    private void updateTouchTargetInsets() {
        if (mModelSelectorButton != null) {
            mModelSelectorButton.setTouchTargetInsets(null, mTopPadding, null, -mTopPadding);
        }
        if (mGlicButton == null || mGlicDismissNudgeButton == null || mGlicActorButton == null) {
            return;
        }
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        float startSlop = GLIC_BUTTON_START_SLOP_DP;
        float endSlop =
                isGlicActorButtonVisible()
                        ? GLIC_BUTTON_WITH_ACTOR_END_SLOP_DP
                        : GLIC_BUTTON_END_SLOP_DP;
        mGlicButton.setTouchTargetInsets(
                -(isRtl ? endSlop : startSlop),
                -GLIC_BUTTON_VERTICAL_SLOP_DP + mTopPadding,
                -(isRtl ? startSlop : endSlop),
                -GLIC_BUTTON_VERTICAL_SLOP_DP - mTopPadding);
        mGlicActorButton.setTouchTargetInsets(
                -(isRtl ? GLIC_BUTTON_END_SLOP_DP : GLIC_ACTOR_START_SLOP_DP),
                -GLIC_BUTTON_VERTICAL_SLOP_DP + mTopPadding,
                -(isRtl ? GLIC_ACTOR_START_SLOP_DP : GLIC_BUTTON_END_SLOP_DP),
                -GLIC_BUTTON_VERTICAL_SLOP_DP - mTopPadding);

        float glicDismissButtonClickSlopDp = getGlicDismissButtonClickSlopDp();
        if (StyleUtils.shouldApplyDesktopDensity()) {
            mGlicDismissNudgeButton.setTouchTargetInsets(
                    -glicDismissButtonClickSlopDp,
                    -glicDismissButtonClickSlopDp + mTopPadding,
                    -glicDismissButtonClickSlopDp,
                    -glicDismissButtonClickSlopDp - mTopPadding);
        } else {
            float endInset =
                    GLIC_BUTTON_END_SLOP_DP
                            + getDimensionDp(R.dimen.tab_strip_glic_button_shortened_end_padding);
            float startInset =
                    StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE
                            - getDimensionDp(R.dimen.tab_strip_glic_dismiss_icon_width)
                            - endInset;
            mGlicDismissNudgeButton.setTouchTargetInsets(
                    -(isRtl ? endInset : startInset),
                    -glicDismissButtonClickSlopDp + mTopPadding,
                    -(isRtl ? startInset : endInset),
                    -glicDismissButtonClickSlopDp - mTopPadding);
        }
    }

    /**
     * Updates the opacity of the Glic buttons based on app focus state.
     *
     * @param isAppInDesktopWindow Whether the app is in a desktop window.
     * @param isTopResumedActivity Whether the app is the top resumed activity.
     */
    public void updateGlicButtonOpacity(
            boolean isAppInDesktopWindow, boolean isTopResumedActivity) {
        mIsAppInDesktopWindow = isAppInDesktopWindow;
        mIsTopResumedActivity = isTopResumedActivity;
        if (mGlicButton == null || mGlicActorButton == null) return;
        float targetOpacity =
                isUnfocusedInDw()
                        ? mContext.getResources()
                                .getFloat(R.dimen.tab_strip_glic_button_icon_unfocused_alpha)
                        : 1.0f;
        mGlicButton.setOpacity(targetOpacity);
        mGlicButton.setClickableOpacityThreshold(targetOpacity);
        mGlicActorButton.setOpacity(targetOpacity);
        mGlicActorButton.setClickableOpacityThreshold(targetOpacity);
    }

    /** Returns the total width used by the trailing buttons including padding. */
    public float getTrailingButtonsWidthWithPadding() {
        float width = 0.0f;
        if (isModelSelectorButtonVisible()) {
            width += StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE;
        }
        if (isGlicButtonVisible()) {
            width += mGlicButton.getWidth() + GLIC_BUTTON_START_SLOP_DP + getGlicButtonEndOffset();

            if (isGlicActorButtonVisible()) {
                width +=
                        getDimensionDp(R.dimen.tab_strip_glic_actor_button_gap)
                                + mGlicActorButton.getWidth();
            }
        }
        return width;
    }

    /** Returns whether the Model Selector Button is currently visible. */
    @EnsuresNonNullIf("mModelSelectorButton")
    public boolean isModelSelectorButtonVisible() {
        return mModelSelectorButton != null && mModelSelectorButton.isVisible();
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

    private void setGlicDismissNudgeButtonVisible(boolean visible) {
        if (mGlicDismissNudgeButton != null) {
            mGlicDismissNudgeButton.setVisible(visible);
        }
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
                setGlicActorButtonText(null, /* forceUpdate= */ false);
                animateGlicButton(
                        mGlicActorButton,
                        0.0f,
                        0.0f,
                        () -> {
                            if (mGlicActorButton != null) {
                                mGlicActorButton.setVisible(false);
                            }
                            updateButtonPositions();
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
     * Fades visible trailing buttons in or out when compositor buttons change visibility (e.g.
     * during tab drag).
     *
     * @param visible Whether the compositor buttons should be visible.
     */
    public void fadeCompositorButtons(boolean visible) {
        float endOpacity = visible ? 1.f : 0.f;
        if (mGlicButton != null && mGlicButton.isVisible()) {
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            mGlicButton,
                            CompositorButton.OPACITY,
                            mGlicButton.getOpacity(),
                            endOpacity,
                            ANIM_BUTTONS_FADE_MS)
                    .start();
        }
        if (mGlicActorButton != null && mGlicActorButton.isVisible()) {
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            mGlicActorButton,
                            CompositorButton.OPACITY,
                            mGlicActorButton.getOpacity(),
                            endOpacity,
                            ANIM_BUTTONS_FADE_MS)
                    .start();
        }
        if (mModelSelectorButton != null && mModelSelectorButton.isVisible()) {
            CompositorAnimator.ofFloatProperty(
                            mUpdateHost.getAnimationHandler(),
                            mModelSelectorButton,
                            CompositorButton.OPACITY,
                            mModelSelectorButton.getOpacity(),
                            endOpacity,
                            ANIM_BUTTONS_FADE_MS)
                    .start();
        }
    }

    /** Returns whether the model selector button should be visible. */
    public boolean shouldModelSelectorButtonBeVisible() {
        if (mModelSelectorButton == null) return false;
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        return selector != null && selector.getModel(true).getCount() != 0;
    }

    /** Returns whether the Glic button should be visible. */
    public boolean shouldGlicBeVisible() {
        if (mGlicButton == null || mProfile == null) {
            return false;
        }
        // TODO(crbug.com/519680563): Remove this side panel check once bottom sheet enabled on LFF.
        if (mSideUiStateProvider == null
                || !mSideUiStateProvider.canShowSideUi(SideUiId.SIDE_PANEL)) {
            return false;
        }
        return GlicEnabling.isEnabledForProfile(mProfile)
                && GlicUtils.isButtonPinnedToTabStrip(mProfile);
    }

    private boolean shouldGlicDismissNudgeBeVisible() {
        return mNudgeLabel != null && shouldGlicBeVisible() && !mIsIncognito;
    }

    /** Returns whether the Glic actor button should be visible. */
    public boolean shouldGlicActorBeVisible() {
        GlicButtonStateController stateController = getOrCreateStateController();
        if (!shouldGlicBeVisible()
                || mGlicActorButton == null
                || stateController == null
                || mIsIncognito) {
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
        if (activity == null || mIsIncognito) return null;

        mStateController =
                new GlicButtonStateController(
                        activity,
                        this::onGlicActorButtonStateChanged,
                        () -> mTaskTracker.get(activity.getTaskId()),
                        /* browserControlsVisibilityManager= */ null);
        if (mProfile != null) {
            mStateController.updateObservations(mProfile);
        }
        return mStateController;
    }

    /** Returns whether the window controls divider should be shown. */
    public boolean shouldShowDivider() {
        return isGlicButtonVisible() && mIsAppInDesktopWindow;
    }

    /**
     * Returns the layout space offset at the end of the Glic button in DP. This includes the base
     * end slop (6dp) to keep touch targets within valid bounds, plus the window controls divider
     * width (2dp) if visible.
     */
    private float getGlicButtonEndOffset() {
        return GLIC_BUTTON_END_SLOP_DP
                + (shouldShowDivider()
                        ? getDimensionDp(R.dimen.tab_strip_window_controls_divider_width)
                        : 0.f);
    }

    private float getDimensionDp(@DimenRes int id) {
        return Math.round(mContext.getResources().getDimension(id) / mDensity);
    }

    private float getGlicDismissButtonClickSlopDp() {
        return (StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE
                        - getDimensionDp(R.dimen.tab_strip_glic_dismiss_icon_width))
                / 2;
    }

    private float getModelSelectorButtonClickSlopDp() {
        return (StripLayoutHelperManager.BUTTON_DESIRED_TOUCH_TARGET_SIZE
                        - getDimensionDp(R.dimen.tab_strip_button_bg_size))
                / 2;
    }

    private boolean isUnfocusedInDw() {
        return mIsAppInDesktopWindow && !mIsTopResumedActivity;
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
        if (mModelSelectorButton != null && mModelSelectorButton.onDown(x, y, buttons)) {
            return true;
        }
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
        if (mModelSelectorButton != null && mModelSelectorButton.onUpOrCancel()) {
            mModelSelectorButtonClickHandler.run();
            return true;
        }
        if (mGlicButton != null) {
            TintedCompositorButton dismissButton = mGlicButton.getDismissButton();
            if (dismissButton != null && dismissButton.isPressed()) {
                dismissButton.onUpOrCancel();
                handleDismissButtonClick();
                return true;
            } else if (mGlicButton.onUpOrCancel()) {
                handleGlicButtonClick(/* preventClose= */ false);
                return true;
            }
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
     * @return True if the event was handled and hit a trailing button.
     */
    public boolean onLongPress(float x, float y) {
        Activity activity = mWindowAndroid.getActivity().get();
        if (activity == null) return false;
        if (mModelSelectorButton != null && mModelSelectorButton.click(x, y, 0)) {
            return true;
        }
        if (mGlicButton != null && mGlicButton.checkClickedOrHovered(x, y)) {
            return mGlicButton.handleLongClick();
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
        boolean msbHovered =
                mModelSelectorButton != null && mModelSelectorButton.checkClickedOrHovered(x, y);
        boolean glicHovered = mGlicButton != null && mGlicButton.checkClickedOrHovered(x, y);
        boolean actorHovered =
                mGlicActorButton != null && mGlicActorButton.checkClickedOrHovered(x, y);
        boolean renderNeeded = false;

        if (mModelSelectorButton != null && msbHovered != mModelSelectorButton.isHovered()) {
            mModelSelectorButton.setHovered(msbHovered);
            renderNeeded = true;
        }
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
        return msbHovered || glicHovered || actorHovered;
    }

    /** Clears hover states on the trailing buttons. */
    public void onHoverExit() {
        boolean renderNeeded = false;
        if (mModelSelectorButton != null && mModelSelectorButton.isHovered()) {
            mModelSelectorButton.setHovered(false);
            renderNeeded = true;
        }
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
        if (mModelSelectorButton != null && mModelSelectorButton.checkClickedOrHovered(x, y)) {
            return true;
        }
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
        if (mModelSelectorButton != null) {
            mModelSelectorButton.drag(x, y);
        }
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
     * @return Whether the event was handled.
     */
    public boolean click(long time, float x, float y, int buttons, int modifiers) {
        if (mModelSelectorButton != null && mModelSelectorButton.checkClickedOrHovered(x, y)) {
            if (mModelSelectorButton.click(x, y, buttons)) {
                mModelSelectorButton.handleClick(time, buttons, modifiers);
                return true;
            }
        }
        if (mGlicButton != null && mGlicButton.checkClickedOrHovered(x, y)) {
            if (MotionEventUtils.isSecondaryClick(buttons)) {
                Activity activity = mWindowAndroid.getActivity().get();
                if (activity != null) {
                    showMenu(activity);
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

    private void handleGlicButtonClick(boolean preventClose) {
        if (mIsIncognito) {
            Activity activity = mWindowAndroid.getActivity().get();
            if (activity != null) {
                GlicHelper.showNotAvailableInIncognitoSnackbar(activity);
            }
            return;
        }
        @GlicInvocationSource int invocationSource = GlicInvocationSource.TOP_CHROME_BUTTON;
        if (mGlicNudgeDelegate.getIsShowingGlicNudge()) {
            invocationSource = GlicInvocationSource.NUDGE;
            mGlicNudgeDelegateBridge.onNudgeActivity(GlicNudgeActivity.NUDGE_CLICKED);
            mGlicNudgeDelegate.onHideGlicNudgeUi();
        }
        mGlicClickHandler.onClick(preventClose, invocationSource);
    }

    private void handleDismissButtonClick() {
        mGlicNudgeDelegateBridge.onNudgeActivity(GlicNudgeActivity.NUDGE_DISMISSED);
        mGlicNudgeDelegate.onHideGlicNudgeUi();
    }

    /* package */ GlicNudgeDelegate getGlicNudgeDelegateForTesting() {
        return mGlicNudgeDelegate;
    }

    /* package */ void setNudgeLabelForTesting(@Nullable String label) {
        mNudgeLabel = label;
        updateTrailingButtonsState(/* animate= */ true, /* forceLayoutChanged= */ false);
    }

    /** Returns the model selector button. */
    public @Nullable TintedCompositorButton getModelSelectorButton() {
        return mModelSelectorButton;
    }

    private void updateModelSelectorButtonProperties() {
        if (mModelSelectorButton == null) return;
        mModelSelectorButton.setIncognito(mIsIncognito);

        @ColorRes
        int iconTintRes =
                mIsIncognito
                        ? R.color.default_icon_color_secondary_light
                        : R.color.default_icon_color_tint_list;
        @ColorRes
        int bgTintRes =
                mIsIncognito
                        ? R.color.tab_strip_msb_bg_incognito_tint_list
                        : R.color.tab_strip_msb_bg_tint_list;
        mModelSelectorButton.setTint(mContext.getColor(iconTintRes));
        mModelSelectorButton.setBackgroundTint(mContext.getColorStateList(bgTintRes));

        mModelSelectorButton.setAccessibilityDescription(
                mIsIncognito
                        ? mContext.getString(
                                R.string.accessibility_tabstrip_btn_incognito_toggle_incognito)
                        : mContext.getString(
                                R.string.accessibility_tabstrip_btn_incognito_toggle_standard));
    }
}
