// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.RectEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.hub.NewTabAnimationUtils;
import org.chromium.chrome.browser.hub.NewTabAnimationUtils.RectStart;
import org.chromium.chrome.browser.hub.RoundedCornerAnimatorUtil;
import org.chromium.chrome.browser.hub.ShrinkExpandAnimator;
import org.chromium.chrome.browser.hub.ShrinkExpandImageView;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuData;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.toolbar.CustomTabCount;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.sensitive_content.SensitiveContentClient;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.ui.animation.RunOnNextLayout;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.util.TokenHolder;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

/**
 * Layout for showing animations when new tabs are created. This is a drop-in replacement for the
 * {@link SimpleAnimationLayout} that uses Android animators rather than compositor animations and
 * uses modern UX designs.
 */
public class NewTabAnimationLayout extends Layout {
    private static final long FOREGROUND_ANIMATION_DURATION_MS = 300L;
    private static final long FOREGROUND_FADE_DURATION_MS = 150L;
    private static final long ANIMATION_TIMEOUT_MS = 800L;
    private final LayoutStateProvider mLayoutStateProvider;
    private final ViewGroup mContentContainer;
    private final ViewGroup mAnimationHostView;
    private final CompositorViewHolder mCompositorViewHolder;
    private final BlackHoleEventFilter mBlackHoleEventFilter;
    private final Handler mHandler;
    private final ToolbarManager mToolbarManager;
    private final ObservableSupplier<Boolean> mScrimVisibilitySupplier;
    private final CustomTabCount mCustomTabCount;
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate;

    private @Nullable StaticTabSceneLayer mSceneLayer;
    private AnimatorSet mTabCreatedForegroundAnimation;
    private AnimatorSet mTabCreatedBackgroundAnimation;
    private ObjectAnimator mFadeAnimator;
    // Retains a strong reference to the {@link ShrinkExpandAnimator} on the class to prevent it
    // from being prematurely GC'd when using {@link ObjectAnimator}.
    private ShrinkExpandAnimator mExpandAnimator;
    private ShrinkExpandImageView mRectView;
    private NewBackgroundTabAnimationHostView mBackgroundHostView;
    private Runnable mAnimationRunnable;
    private Runnable mTimeoutRunnable;
    private Callback<Boolean> mVisibilityObserver;
    private @TabId int mNextTabId = Tab.INVALID_TAB_ID;
    private int mToken = TokenHolder.INVALID_TOKEN;
    private boolean mSkipForceAnimationToFinish;
    private boolean mRunOnNextLayoutImmediatelyForTesting;

    /**
     * Creates an instance of the {@link NewTabAnimationLayout}.
     *
     * @param context The current Android's context.
     * @param updateHost The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost The {@link LayoutRenderHost} view for this layout.
     * @param layoutStateProvider Provider for layout state updates.
     * @param contentContainer The container for content sensitivity.
     * @param compositorViewHolderSupplier Supplier to the {@link CompositorViewHolder} instance.
     * @param animationHostView The host view for animations.
     * @param toolbarManager The {@link ToolbarManager} instance.
     * @param scrimVisibilitySupplier Supplier for the Scrim visibility.
     */
    public NewTabAnimationLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            LayoutStateProvider layoutStateProvider,
            ViewGroup contentContainer,
            ObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            ViewGroup animationHostView,
            ToolbarManager toolbarManager,
            BrowserControlsManager browserControlsManager,
            ObservableSupplier<Boolean> scrimVisibilitySupplier) {
        super(context, updateHost, renderHost);
        mLayoutStateProvider = layoutStateProvider;
        mContentContainer = contentContainer;
        mCompositorViewHolder = compositorViewHolderSupplier.get();
        mBlackHoleEventFilter = new BlackHoleEventFilter(context);
        mAnimationHostView = animationHostView;
        mHandler = new Handler();
        mToolbarManager = toolbarManager;
        mScrimVisibilitySupplier = scrimVisibilitySupplier;
        mCustomTabCount = mToolbarManager.getCustomTabCount();
        mBrowserVisibilityDelegate = browserControlsManager.getBrowserVisibilityDelegate();
    }

    @Override
    public void onFinishNativeInitialization() {
        ensureSceneLayerExists();
    }

    @Override
    public void destroy() {
        if (mSceneLayer != null) {
            mSceneLayer.destroy();
            mSceneLayer = null;
        }
    }

    @Override
    public void setTabContentManager(TabContentManager tabContentManager) {
        super.setTabContentManager(tabContentManager);
        if (mSceneLayer != null && tabContentManager != null) {
            mSceneLayer.setTabContentManager(tabContentManager);
        }
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesTabClosing() {
        return false;
    }

    @Override
    protected EventFilter getEventFilter() {
        return mBlackHoleEventFilter;
    }

    @Override
    public SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public int getLayoutType() {
        return LayoutType.SIMPLE_ANIMATION;
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        mNextTabId = Tab.INVALID_TAB_ID;
        reset();

        if (mTabModelSelector == null || mTabContentManager == null) return;

        @Nullable Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null && tab.isNativePage()) {
            mTabContentManager.cacheTabThumbnail(tab);
        }
    }

    @Override
    public void doneHiding() {
        TabModelUtils.selectTabById(mTabModelSelector, mNextTabId, TabSelectionType.FROM_USER);
        super.doneHiding();
        updateAnimationHostViewSensitivity(Tab.INVALID_TAB_ID);
    }

    @Override
    protected void forceAnimationToFinish() {
        if (mSkipForceAnimationToFinish) {
            mSkipForceAnimationToFinish = false;
        } else {
            runQueuedRunnableIfExists();
            if (mTabCreatedForegroundAnimation != null) mTabCreatedForegroundAnimation.end();
            if (mTabCreatedBackgroundAnimation != null) mTabCreatedBackgroundAnimation.end();
        }
    }

    @Override
    public void onTabCreating(@TabId int sourceTabId) {
        reset();

        ensureSourceTabCreated(sourceTabId);
        updateAnimationHostViewSensitivity(sourceTabId);
    }

    @Override
    public void onTabCreated(
            long time,
            @TabId int id,
            int index,
            @TabId int sourceId,
            boolean newIsIncognito,
            boolean background,
            float originX,
            float originY) {
        assert mTabModelSelector != null;
        Tab newTab = mTabModelSelector.getModel(newIsIncognito).getTabById(id);
        if (newTab != null
                && newTab.getLaunchType() == TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP) {
            // Tab selection will no-op for Tab.INVALID_TAB_ID. This operation should not change
            // the current tab. If for some reason this is the last tab it will be automatically
            // selected.
            mNextTabId = Tab.INVALID_TAB_ID;
            startHiding();
            return;
        }

        ensureSourceTabCreated(sourceId);
        updateAnimationHostViewSensitivity(sourceId);
        @Nullable Tab oldTab = mTabModelSelector.getTabById(sourceId);

        if (background && oldTab != null) {
            Context context = getContext();
            boolean isRegularNtp =
                    (oldTab.getUrl() != null)
                            && UrlUtilities.isNtpUrl(oldTab.getUrl())
                            && !oldTab.isIncognitoBranded();

            @Nullable TabContextMenuData data = TabContextMenuData.getForTab(oldTab);
            int defaultX = Math.round(mAnimationHostView.getWidth() / 2f);
            int defaultY = Math.round(mAnimationHostView.getHeight() / 2f);
            @Nullable Point point;
            @Px int x;
            @Px int y;
            if (isRegularNtp) {
                point = ((NewTabPage) oldTab.getNativePage()).getLastTouchPosition();
                x = point.x != -1 ? point.x : defaultX;
                y = point.y != -1 ? point.y : defaultY;
            } else {
                point = data == null ? null : data.getLastTriggeringTouchPositionDp();
                if (point != null) {
                    x = ViewUtils.dpToPx(context, point.x);
                    y = ViewUtils.dpToPx(context, point.y);
                } else {
                    x = defaultX;
                    y = defaultY;
                }
            }

            ObservableSupplier<Boolean> visibilitySupplier =
                    data != null && !isRegularNtp
                            ? data.getTabContextMenuVisibilitySupplier()
                            : mScrimVisibilitySupplier;
            tabCreatedInBackground(oldTab, isRegularNtp, x, y, visibilitySupplier);
        } else {
            tabCreatedInForeground(
                    id, sourceId, newIsIncognito, getForegroundRectStart(oldTab, newTab));
        }
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayerExists();
        if (!hasLayoutTab()) return;

        boolean needUpdate = updateSnap(dt, getLayoutTab());
        if (needUpdate) requestUpdate();
    }

    @Override
    protected void updateSceneLayer(
            RectF viewport,
            RectF contentViewport,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        ensureSceneLayerExists();

        LayoutTab layoutTab = getLayoutTab();
        layoutTab.set(LayoutTab.IS_ACTIVE_LAYOUT, isActive());
        layoutTab.set(LayoutTab.CONTENT_OFFSET, browserControls.getContentOffset());
        mSceneLayer.update(layoutTab);
    }

    /**
     * Returns true if the foreground animation is running (excluding {@link #mFadeAnimator}).
     *
     * <p>Including {@link #mFadeAnimator} would prevent {@link #doneHiding} from being called
     * during the animation cycle in {@link
     * org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl#onUpdate(long, long)}.
     *
     * <p>There is also a race condition in {@link #tabCreatedInBackground} where {@link
     * org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl#onUpdate(long, long)} gets
     * called when the animation already started, causing the layout to freeze. Hence, we skip this
     * check for the background animation.
     */
    @Override
    public boolean isRunningAnimations() {
        return mTabCreatedForegroundAnimation != null;
    }

    private void reset() {
        mLayoutTabs = null;
    }

    private boolean hasLayoutTab() {
        return mLayoutTabs != null && mLayoutTabs.length > 0;
    }

    private LayoutTab getLayoutTab() {
        assert hasLayoutTab();
        return mLayoutTabs[0];
    }

    private void ensureSceneLayerExists() {
        if (mSceneLayer != null) return;

        mSceneLayer = new StaticTabSceneLayer();
        if (mTabContentManager == null) return;

        mSceneLayer.setTabContentManager(mTabContentManager);
    }

    private void ensureSourceTabCreated(@TabId int sourceTabId) {
        if (hasLayoutTab() && mLayoutTabs[0].getId() == sourceTabId) return;

        @Nullable Tab tab = mTabModelSelector.getTabById(sourceTabId);
        if (tab == null) return;
        LayoutTab sourceLayoutTab = createLayoutTab(sourceTabId, tab.isIncognitoBranded());

        mLayoutTabs = new LayoutTab[] {sourceLayoutTab};
        updateCacheVisibleIds(Collections.singletonList(sourceTabId));
    }

    private void updateAnimationHostViewSensitivity(@TabId int sourceTabId) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.VANILLA_ICE_CREAM
                || !ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                || !ChromeFeatureList.isEnabled(
                        SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
            return;
        }

        if (sourceTabId != TabModel.INVALID_TAB_INDEX) {
            // This code can be reached from both {@link NewTabAnimationLayout#onTabCreating}
            // and {@link NewTabAnimationLayout#onTabCreated}. If the content container is
            // already sensitive, there is no need to mark it as sensitive again.
            if (mContentContainer.getContentSensitivity() == View.CONTENT_SENSITIVITY_SENSITIVE) {
                return;
            }
            @Nullable Tab tab = mTabModelSelector.getTabById(sourceTabId);
            if (tab == null || !tab.getTabHasSensitiveContent()) {
                return;
            }
            mContentContainer.setContentSensitivity(View.CONTENT_SENSITIVITY_SENSITIVE);
            RecordHistogram.recordEnumeratedHistogram(
                    "SensitiveContent.SensitiveTabSwitchingAnimations",
                    SensitiveContentClient.TabSwitchingAnimation.NEW_TAB_IN_BACKGROUND,
                    SensitiveContentClient.TabSwitchingAnimation.COUNT);
        } else {
            mContentContainer.setContentSensitivity(View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
        }
    }

    /**
     * Gets the position where the {@link #mRectView} should start from for the new foreground tab
     * animation.
     *
     * @param oldTab The current {@link Tab}.
     * @param newTab The new {@link Tab} to animate.
     */
    private @RectStart int getForegroundRectStart(Tab oldTab, Tab newTab) {
        @TabLaunchType int tabLaunchType = newTab.getLaunchType();
        if (oldTab == null
                || tabLaunchType == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || tabLaunchType == TabLaunchType.FROM_LONGPRESS_FOREGROUND_IN_GROUP) {
            return RectStart.CENTER;
        }

        boolean oldTabHasTopToolbar = ToolbarPositionController.shouldShowToolbarOnTop(oldTab);
        boolean newTabHasTopToolbar = ToolbarPositionController.shouldShowToolbarOnTop(newTab);

        if (oldTabHasTopToolbar && newTabHasTopToolbar) {
            return RectStart.TOP_TOOLBAR;
        } else if (oldTabHasTopToolbar) {
            return RectStart.TOP;
        } else if (newTabHasTopToolbar) {
            return RectStart.BOTTOM;
        } else {
            return RectStart.BOTTOM_TOOLBAR;
        }
    }

    /**
     * Runs the queued runnable immediately, if it exists.
     *
     * <p>It checks for and executes either {@link #mTimeoutRunnable} or {@link #mAnimationRunnable}
     * and removes the queued timeout runnable from {@link #mHandler}. If {@link
     * #mAnimationRunnable} is found, it calls {@link RunOnNextLayout#runOnNextLayoutRunnables()} in
     * the View to run {@link #mAnimationRunnable} and ensure a valid animation status before
     * calling {@link AnimatorSet#end()}.
     */
    private void runQueuedRunnableIfExists() {
        if (mTimeoutRunnable != null) {
            mHandler.removeCallbacks(mTimeoutRunnable);
            mTimeoutRunnable.run();
        } else if (mAnimationRunnable != null) {
            if (mRectView != null) mRectView.runOnNextLayoutRunnables();
            if (mBackgroundHostView != null) mBackgroundHostView.runOnNextLayoutRunnables();
        }
        assert mTimeoutRunnable == null : "Timeout runnable exists";
        assert mAnimationRunnable == null : "Animation runnable exists";
    }

    private void setRunOnNextLayout(RunOnNextLayout view, Runnable r) {
        view.runOnNextLayout(r);
        if (mRunOnNextLayoutImmediatelyForTesting) view.runOnNextLayoutRunnables();
    }

    /**
     * Forces the new tab animation to finish.
     *
     * <p>This method is intended for internal use within {@link NewTabAnimationLayout}. It ensures
     * {@link #mFadeAnimator} runs after calling {@link #startHiding}, preventing premature
     * termination by external calls to {@link #forceAnimationToFinish} from {@link
     * org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl#startShowing}.
     */
    @VisibleForTesting
    void forceNewTabAnimationToFinish() {
        // TODO(crbug.com/40933120): Make sure the right mode is selected after forcing the
        // animation to finish.
        runQueuedRunnableIfExists();
        if (mTabCreatedForegroundAnimation != null) {
            mAnimationHostView.removeView(mRectView);
            mRectView = null;
            mFadeAnimator = null;
            mTabCreatedForegroundAnimation.end();
        } else if (mFadeAnimator != null) {
            mFadeAnimator.end();
        }
        if (mTabCreatedBackgroundAnimation != null) mTabCreatedBackgroundAnimation.end();
        mSkipForceAnimationToFinish = false;
    }

    /**
     * Animates opening a tab in the foreground.
     *
     * @param id The id of the new tab to animate.
     * @param sourceId The id of the tab that spawned this new tab.
     * @param newIsIncognito True if the new tab is an incognito tab.
     * @param rectStart Origin point where the animation starts.
     */
    private void tabCreatedInForeground(
            @TabId int id, @TabId int sourceId, boolean newIsIncognito, @RectStart int rectStart) {
        LayoutTab newLayoutTab = createLayoutTab(id, newIsIncognito);
        if (mLayoutTabs == null || mLayoutTabs.length == 0) {
            mLayoutTabs = new LayoutTab[] {newLayoutTab};
            updateCacheVisibleIds(Collections.singletonList(id));
        } else {
            mLayoutTabs = new LayoutTab[] {mLayoutTabs[0], newLayoutTab};
            updateCacheVisibleIds(new ArrayList<>(Arrays.asList(id, sourceId)));
        }
        forceNewTabAnimationToFinish();

        // TODO(crbug.com/40933120): Investigate why the old tab flickers when switching to the new
        // tab.
        requestUpdate();

        Context context = getContext();
        mRectView = new ShrinkExpandImageView(context);
        @ColorInt
        int backgroundColor = NewTabAnimationUtils.getBackgroundColor(context, newIsIncognito);
        mRectView.setRoundedFillColor(backgroundColor);

        // TODO(crbug.com/40933120): Investigate why {@link
        // RoundedCornerImageView#setRoundedCorners} sometimes incorrectly detects the view as LTR
        // during the animation.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        mRectView.setLayoutDirection(isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);

        Rect initialRect = new Rect();
        Rect finalRect = new Rect();
        Rect hostViewRect = new Rect();
        mAnimationHostView.getGlobalVisibleRect(hostViewRect);

        int radius =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.new_tab_animation_rect_corner_radius);
        int[] startRadii = new int[4];
        Arrays.fill(startRadii, radius);

        if (rectStart != RectStart.CENTER) {
            RectF compositorViewportRectf = new RectF();
            mCompositorViewHolder.getVisibleViewport(compositorViewportRectf);
            compositorViewportRectf.round(finalRect);

            // Without adding/subtracting 1px, the origin corner shows a bit of blinking when
            // running the animation. Doing so ensures the {@link ShrinkExpandImageView} fully
            // covers the origin corner.
            if (rectStart == RectStart.TOP || rectStart == RectStart.TOP_TOOLBAR) {
                startRadii[0] = 0;
                mCompositorViewHolder.getWindowViewport(compositorViewportRectf);
                finalRect.bottom = Math.round(compositorViewportRectf.bottom);
                finalRect.top =
                        rectStart == RectStart.TOP ? hostViewRect.top - 1 : finalRect.top - 1;
            } else {
                startRadii[2] = 0;
                finalRect.top = hostViewRect.top;
                finalRect.bottom =
                        rectStart == RectStart.BOTTOM
                                ? hostViewRect.bottom + 1
                                : finalRect.bottom + 1;
            }
            if (isRtl) {
                finalRect.right += 1;
            } else {
                finalRect.left -= 1;
            }
        } else {
            finalRect = hostViewRect;
            Rect compositorViewRect = new Rect();
            mCompositorViewHolder.getGlobalVisibleRect(compositorViewRect);
            finalRect.bottom -= compositorViewRect.top;
        }

        NewTabAnimationUtils.updateRects(rectStart, isRtl, initialRect, finalRect);

        mExpandAnimator =
                new ShrinkExpandAnimator(
                        mRectView, initialRect, finalRect, /* searchBoxHeight= */ 0);
        ObjectAnimator rectAnimator =
                ObjectAnimator.ofObject(
                        mExpandAnimator,
                        ShrinkExpandAnimator.RECT,
                        new RectEvaluator(),
                        initialRect,
                        finalRect);

        float scaleFactor = (float) initialRect.width() / finalRect.width();
        int[] endRadii = new int[4];
        for (int i = 0; i < 4; ++i) {
            endRadii[i] = Math.round(startRadii[i] * scaleFactor);
        }
        mRectView.setRoundedCorners(startRadii[0], startRadii[1], startRadii[2], startRadii[3]);
        ValueAnimator cornerAnimator =
                RoundedCornerAnimatorUtil.createRoundedCornerAnimator(
                        mRectView, startRadii, endRadii);

        mFadeAnimator = ObjectAnimator.ofFloat(mRectView, ShrinkExpandImageView.ALPHA, 1f, 0f);
        mFadeAnimator.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        mFadeAnimator.setDuration(FOREGROUND_FADE_DURATION_MS);
        mFadeAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mFadeAnimator = null;
                        mAnimationHostView.removeView(mRectView);
                        mRectView = null;
                    }
                });

        mTabCreatedForegroundAnimation = new AnimatorSet();
        mTabCreatedForegroundAnimation.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        mTabCreatedForegroundAnimation.setDuration(FOREGROUND_ANIMATION_DURATION_MS);
        mTabCreatedForegroundAnimation.playTogether(rectAnimator, cornerAnimator);
        mTabCreatedForegroundAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mTabCreatedForegroundAnimation = null;
                        mExpandAnimator = null;
                        if (mFadeAnimator != null) mFadeAnimator.start();
                        startHiding();
                        mTabModelSelector.selectModel(newIsIncognito);
                        mNextTabId = id;
                    }
                });
        mAnimationRunnable =
                () -> {
                    mAnimationRunnable = null;
                    // Make View visible once the animation is ready to start.
                    mRectView.setVisibility(View.VISIBLE);
                    mTabCreatedForegroundAnimation.start();
                };

        // {@link View#INVISIBLE} is needed to generate the geometry information.
        mRectView.setVisibility(View.INVISIBLE);
        mAnimationHostView.addView(mRectView);
        mRectView.reset(initialRect);
        setRunOnNextLayout(mRectView, mAnimationRunnable);
    }

    /**
     * Animates opening a tab in the background.
     *
     * @param animationTab The tab being animated over.
     * @param isRegularNtp True if the old tab is regular NTP.
     * @param x The x coordinate of the originating touch input in px.
     * @param y The y coordinate of the originating touch input in px.
     * @param visibilitySupplier The visibility supplier for either the context menu or the NTP
     *     bottom sheet's scrim.
     */
    private void tabCreatedInBackground(
            Tab animationTab,
            boolean isRegularNtp,
            @Px int x,
            @Px int y,
            ObservableSupplier<Boolean> visibilitySupplier) {
        boolean isIncognito = animationTab.isIncognitoBranded();
        assert mLayoutTabs.length == 1;
        forceNewTabAnimationToFinish();
        mSkipForceAnimationToFinish = true;
        startHiding();

        if (!isRegularNtp && mToken == TokenHolder.INVALID_TOKEN) {
            mToken = mBrowserVisibilityDelegate.showControlsPersistent();
        }

        ToggleTabStackButton tabSwitcherButton =
                mAnimationHostView.findViewById(R.id.tab_switcher_button);
        assert tabSwitcherButton != null;

        Context context = getContext();
        mBackgroundHostView =
                (NewBackgroundTabAnimationHostView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.new_background_tab_animation_host_view,
                                        mAnimationHostView,
                                        false);
        int prevTabCount = mTabModelSelector.getModel(isIncognito).getCount() - 1;
        mCustomTabCount.set(prevTabCount);
        @ColorInt
        int toolbarColor =
                isRegularNtp
                        ? NewTabAnimationUtils.getBackgroundColor(context, isIncognito)
                        : mToolbarManager.getPrimaryColor();

        int[] toolbarPosition = new int[2];
        mAnimationHostView.findViewById(R.id.toolbar).getLocationInWindow(toolbarPosition);
        Rect compositorViewRect = new Rect();
        mCompositorViewHolder.getGlobalVisibleRect(compositorViewRect);
        boolean isTopToolbar =
                isRegularNtp || ToolbarPositionController.shouldShowToolbarOnTop(animationTab);

        mBackgroundHostView.setUpAnimation(
                tabSwitcherButton,
                isRegularNtp,
                isIncognito,
                isTopToolbar,
                toolbarColor,
                prevTabCount,
                toolbarPosition[1],
                compositorViewRect.top,
                compositorViewRect.left,
                mToolbarManager.getNtpTransitionPercentage());

        // {@link View#INVISIBLE} is needed to generate the geometry information.
        mBackgroundHostView.setVisibility(View.INVISIBLE);
        mAnimationHostView.addView(mBackgroundHostView);

        // This ensures the view to be properly laid out in order to do calculations within the
        // background animation host view. The main reason we need this is to get values from
        // {@link NewBackgroundTabSwitcherButton#getButtonLocation}.
        mAnimationRunnable =
                () -> {
                    mAnimationRunnable = null;
                    mTimeoutRunnable = null;
                    AnimationInterruptor interruptor =
                            new AnimationInterruptor(
                                    mLayoutStateProvider,
                                    mTabModelSelector.getCurrentTabSupplier(),
                                    animationTab,
                                    mScrimVisibilitySupplier,
                                    this::forceNewTabAnimationToFinish);
                    mTabCreatedBackgroundAnimation = mBackgroundHostView.getAnimatorSet(x, y);
                    mTabCreatedBackgroundAnimation.addListener(
                            new AnimatorListenerAdapter() {
                                @Override
                                public void onAnimationStart(Animator animation) {
                                    // Release custom tab count as soon as the animation starts to
                                    // avoid showing the old tab count if the user decides to scroll
                                    // up during AnimationType.NTP_PARTIAL_SCROLL or
                                    // AnimationType.NTP_FULL_SCROLL.
                                    mCustomTabCount.release();
                                }

                                @Override
                                public void onAnimationEnd(Animator animation) {
                                    interruptor.destroy();
                                    cleanUpAnimation();
                                }
                            });
                    mBackgroundHostView.setVisibility(View.VISIBLE);
                    mTabCreatedBackgroundAnimation.start();
                };

        mTimeoutRunnable =
                () -> {
                    if (mTimeoutRunnable == null) return;
                    mTimeoutRunnable = null;
                    mAnimationRunnable = null;
                    cleanUpAnimation();
                    mCustomTabCount.release();
                    if (mVisibilityObserver != null) {
                        visibilitySupplier.removeObserver(mVisibilityObserver);
                        mVisibilityObserver = null;
                    }
                };

        mVisibilityObserver =
                visible -> {
                    if (!visible) {
                        mHandler.removeCallbacks(mTimeoutRunnable);
                        mTimeoutRunnable = null;
                        setRunOnNextLayout(mBackgroundHostView, mAnimationRunnable);
                        visibilitySupplier.removeObserver(mVisibilityObserver);
                        mVisibilityObserver = null;
                    }
                };

        if (visibilitySupplier.get()) {
            visibilitySupplier.addObserver(mVisibilityObserver);
            // TODO(crbug.com/40282469): Check with UX about the NTP bottom sheet's scrim taking a
            // bit to disappear and decrease the timeout.
            mHandler.postDelayed(mTimeoutRunnable, ANIMATION_TIMEOUT_MS);
        } else {
            setRunOnNextLayout(mBackgroundHostView, mAnimationRunnable);
        }
    }

    private void cleanUpAnimation() {
        mTabCreatedBackgroundAnimation = null;
        mAnimationHostView.removeView(mBackgroundHostView);
        mBackgroundHostView = null;
        if (mToken != TokenHolder.INVALID_TOKEN) {
            mBrowserVisibilityDelegate.releasePersistentShowingToken(mToken);
            mToken = TokenHolder.INVALID_TOKEN;
        }
    }

    protected void setRunOnNextLayoutImmediatelyForTesting(boolean runImmediately) {
        mRunOnNextLayoutImmediatelyForTesting = runImmediately;
    }

    protected void setNextTabIdForTesting(@TabId int nextTabId) {
        mNextTabId = nextTabId;
    }
}
