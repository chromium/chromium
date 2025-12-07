// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.layouts.phone.NewBackgroundTabAnimationHostView.AnimationType;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.hub.NewTabAnimationUtils;
import org.chromium.chrome.browser.hub.NewTabAnimationUtils.RectStart;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.edge_to_edge.TopInsetCoordinator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuData;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.toolbar.CustomTabCount;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.top.ToggleTabStackButton;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.sensitive_content.SensitiveContentClient;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.ui.animation.RunOnNextLayout;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
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
@NullMarked
public class NewTabAnimationLayout extends Layout {
    private static final long ANIMATION_TIMEOUT_MS = 800L;
    private static final String TAG = "NTAnimLayout";
    private final boolean mLogsEnabled;
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
    private final ObservableSupplier<TopInsetCoordinator> mTopInsetCoordinatorSupplier;
    private final Callback<TopInsetCoordinator> mTopInsetCoordinatorObserver =
            this::onTopInsetCoordinatorAvailable;

    private @Nullable StaticTabSceneLayer mSceneLayer;
    private @Nullable NewBackgroundTabAnimationHostView mBackgroundHostView;
    private @Nullable NewForegroundTabAnimationHostView mForegroundHostView;
    private @Nullable AnimatorSet mTabCreatedBackgroundAnimation;
    // The real tab switcher button view. This is used to update the view visibility based on the
    // background animation progress.
    private @Nullable View mTabSwitcherButton;
    private @Nullable TopInsetCoordinator mTopInsetCoordinator;
    private @Nullable Runnable mAnimationRunnable;
    private @Nullable Runnable mTimeoutRunnable;
    private @Nullable Callback<Boolean> mVisibilityObserver;
    private @TabId int mNextTabId = Tab.INVALID_TAB_ID;
    private int mBrowserControlsVisibilityToken = TokenHolder.INVALID_TOKEN;
    private int mCustomTabCountToken = TokenHolder.INVALID_TOKEN;
    private int mTopPadding;
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
     * @param browserControlsManager The {@link BrowserControlsManager} instance.
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
            ObservableSupplier<Boolean> scrimVisibilitySupplier,
            ObservableSupplier<TopInsetCoordinator> topInsetCoordinatorSupplier) {
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
        mLogsEnabled = ChromeFeatureList.sShowNewTabAnimationsLogs.getValue();
        mTopInsetCoordinatorSupplier = topInsetCoordinatorSupplier;
        topInsetCoordinatorSupplier.addSyncObserverAndCallIfNonNull(mTopInsetCoordinatorObserver);
    }

    @Override
    public void onFinishNativeInitialization() {
        ensureSceneLayerExists();
    }

    @Override
    public void destroy() {
        if (mTopInsetCoordinator == null) {
            mTopInsetCoordinatorSupplier.removeObserver(mTopInsetCoordinatorObserver);
        }
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
    public @Nullable SceneLayer getSceneLayer() {
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
        assumeNonNull(mTabModelSelector);
        TabModelUtils.selectTabById(mTabModelSelector, mNextTabId, TabSelectionType.FROM_USER);
        super.doneHiding();
        updateAnimationHostViewSensitivity(Tab.INVALID_TAB_ID);
    }

    @Override
    protected void forceAnimationToFinish() {
        if (mSkipForceAnimationToFinish) {
            if (mLogsEnabled) Log.i(TAG, "forceAnimationToFinish: skipped");
            mSkipForceAnimationToFinish = false;
            return;
        }
        runQueuedRunnableIfExists();
        if (mForegroundHostView != null) {
            if (mLogsEnabled) {
                Log.i(TAG, "forceAnimationToFinish: mForegroundHostView#forceAnimationToFinish");
            }
            mForegroundHostView.forceAnimationToFinish();
            mAnimationHostView.removeView(mForegroundHostView);
            mForegroundHostView = null;
        }
        if (mTabCreatedBackgroundAnimation != null) {
            if (mLogsEnabled) {
                Log.i(TAG, "forceAnimationToFinish: mTabCreatedBackgroundAnimation#end");
            }
            mTabCreatedBackgroundAnimation.end();
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
        mTopPadding = 0;
        Tab newTab = mTabModelSelector.getModel(newIsIncognito).getTabById(id);
        if (newTab != null
                && (newTab.getLaunchType() == TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP
                        || newTab.getLaunchType() == TabLaunchType.FROM_TIPS_NOTIFICATIONS)) {
            // Tab selection will no-op for Tab.INVALID_TAB_ID. This operation should not change
            // the current tab. If for some reason this is the last tab it will be automatically
            // selected.
            mNextTabId = Tab.INVALID_TAB_ID;
            startHiding();
            return;
        }

        ensureSourceTabCreated(sourceId);
        updateAnimationHostViewSensitivity(sourceId);
        mSkipForceAnimationToFinish = false;
        if (mLogsEnabled) Log.i(TAG, "onTabCreated: forceAnimationToFinish");
        forceAnimationToFinish();
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
                point = assumeNonNull((NewTabPage) oldTab.getNativePage()).getLastTouchPosition();
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
            assumeNonNull(newTab);
            mTopPadding = getTopInsetIfNeeded(newTab);
            tabCreatedInForeground(id, sourceId, newIsIncognito, oldTab, newTab);
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
        if (!hasLayoutTab()) return;

        LayoutTab layoutTab = getLayoutTab();
        layoutTab.set(LayoutTab.IS_ACTIVE_LAYOUT, isActive());
        layoutTab.set(LayoutTab.CONTENT_OFFSET, browserControls.getContentOffset() + mTopPadding);
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
        boolean isRunning =
                mForegroundHostView != null && mForegroundHostView.isExpandAnimationRunning();
        return isRunning;
    }

    @Override
    public void startHiding() {
        if (mLogsEnabled) Log.i(TAG, "startHiding");
        super.startHiding();
    }

    private void reset() {
        mLayoutTabs = null;
    }

    @EnsuresNonNullIf({"mLayoutTabs"})
    private boolean hasLayoutTab() {
        return mLayoutTabs != null && mLayoutTabs.length > 0;
    }

    private LayoutTab getLayoutTab() {
        assert hasLayoutTab();
        return mLayoutTabs[0];
    }

    @EnsuresNonNull({"mSceneLayer"})
    private void ensureSceneLayerExists() {
        if (mSceneLayer != null) return;

        mSceneLayer = new StaticTabSceneLayer();
        if (mTabContentManager == null) return;

        mSceneLayer.setTabContentManager(mTabContentManager);
    }

    private void ensureSourceTabCreated(@TabId int sourceTabId) {
        if (hasLayoutTab() && mLayoutTabs[0].getId() == sourceTabId) return;

        assumeNonNull(mTabModelSelector);
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

        assumeNonNull(mTabModelSelector);
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
    private @RectStart int getForegroundRectStart(@Nullable Tab oldTab, Tab newTab) {
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
            if (mForegroundHostView != null) {
                mForegroundHostView.runOnNextLayoutRunnables();
                if (mLogsEnabled) {
                    Log.i(
                            TAG,
                            "runQueuedRunnableIfExists:"
                                    + " mForegroundHostView#runOnNextLayoutRunnables");
                }
            }
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
     * Animates opening a tab in the foreground.
     *
     * @param id The id of the new tab to animate.
     * @param sourceId The id of the tab that spawned this new tab.
     * @param newIsIncognito True if the new tab is an incognito tab.
     * @param oldTab The current {@link Tab}.
     * @param newTab The new {@link Tab} to animate.
     */
    private void tabCreatedInForeground(
            @TabId int id,
            @TabId int sourceId,
            boolean newIsIncognito,
            @Nullable Tab oldTab,
            Tab newTab) {
        LayoutTab newLayoutTab = createLayoutTab(id, newIsIncognito);
        if (mLayoutTabs == null || mLayoutTabs.length == 0) {
            mLayoutTabs = new LayoutTab[] {newLayoutTab};
            updateCacheVisibleIds(Collections.singletonList(id));
        } else {
            mLayoutTabs = new LayoutTab[] {mLayoutTabs[0], newLayoutTab};
            updateCacheVisibleIds(new ArrayList<>(Arrays.asList(id, sourceId)));
        }

        // TODO(crbug.com/463341238): Investigate why the old tab flickers when switching to the new
        // tab.
        requestUpdate();

        Context context = getContext();

        boolean isRtl = LocalizationUtils.isLayoutRtl();

        int radius =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.new_tab_animation_rect_corner_radius);
        int[] startRadii = new int[4];
        Arrays.fill(startRadii, radius);

        Rect initialRect = new Rect();
        Rect finalRect = new Rect();
        Rect hostViewRect = new Rect();
        mAnimationHostView.getGlobalVisibleRect(hostViewRect);
        @RectStart int rectStart = getForegroundRectStart(oldTab, newTab);

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
                finalRect.top = rectStart == RectStart.TOP ? -1 : finalRect.top - 1;
            } else {
                startRadii[2] = 0;
                finalRect.bottom =
                        rectStart == RectStart.BOTTOM
                                ? hostViewRect.bottom + 1
                                : finalRect.bottom + 1;

                // 0 instead of -1 since the rect is not expanding from this corner.
                finalRect.top = 0;
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

            // 0 instead of -1 since the rect is not expanding from this corner.
            finalRect.top = 0;
        }

        if (mTopInsetCoordinator != null) {
            // Adjust rect proportions for top padding in E2E.
            final boolean isNewTabE2E = mTopPadding > 0;
            final boolean isOldTabE2E = NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(oldTab);
            if (isNewTabE2E && isOldTabE2E) {
                // Case: E2E -> E2E.
                finalRect.top += mTopPadding;
            } else if (isNewTabE2E) {
                // Case: non-E2E -> E2E, or null -> E2E.
                finalRect.offset(0, mTopPadding);
            } else if (isOldTabE2E) {
                // Case: E2E -> non-E2E.
                finalRect.bottom -= mTopInsetCoordinator.getSystemTopInset();
            }
        }

        NewTabAnimationUtils.updateRects(rectStart, isRtl, initialRect, finalRect);

        float scaleFactor = (float) initialRect.width() / finalRect.width();
        int[] endRadii = new int[4];
        for (int i = 0; i < 4; ++i) {
            endRadii[i] = Math.round(startRadii[i] * scaleFactor);
        }

        @ColorInt
        int backgroundColor = NewTabAnimationUtils.getBackgroundColor(context, newIsIncognito);

        NewForegroundTabAnimationHostView.Listener listener =
                new NewForegroundTabAnimationHostView.Listener() {
                    @Override
                    public void onExpandAnimationFinished() {
                        if (mLogsEnabled) Log.i(TAG, "Listener: onExpandAnimationFinished");
                        mSkipForceAnimationToFinish = true;
                        startHiding();
                        assumeNonNull(mTabModelSelector);
                        mTabModelSelector.selectModel(newIsIncognito);
                        mNextTabId = id;
                    }

                    @Override
                    public void onForegroundAnimationFinished() {
                        if (mLogsEnabled) Log.i(TAG, "Listener: onForegroundAnimationFinished");
                        assumeNonNull(mForegroundHostView);
                        mAnimationHostView.removeView(mForegroundHostView);
                        mForegroundHostView = null;
                    }
                };

        final Rect finalAnimationRect = new Rect(finalRect);
        mAnimationRunnable =
                () -> {
                    mAnimationRunnable = null;
                    // Make View visible once the animation is ready to start.
                    assumeNonNull(mForegroundHostView);
                    mForegroundHostView.startAnimation(finalAnimationRect, endRadii);
                };

        mForegroundHostView =
                new NewForegroundTabAnimationHostView(
                        context,
                        initialRect,
                        startRadii,
                        backgroundColor,
                        isRtl,
                        listener,
                        mLogsEnabled);
        mAnimationHostView.addView(mForegroundHostView);
        setRunOnNextLayout(mForegroundHostView, mAnimationRunnable);
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
        assert assumeNonNull(mLayoutTabs).length == 1;
        mSkipForceAnimationToFinish = true;
        forceHidingImmediatelyIfNeeded(isRegularNtp);

        if (!isRegularNtp && mBrowserControlsVisibilityToken == TokenHolder.INVALID_TOKEN) {
            mBrowserControlsVisibilityToken = mBrowserVisibilityDelegate.showControlsPersistent();
        }

        ToggleTabStackButton tabSwitcherButton =
                mAnimationHostView.findViewById(R.id.tab_switcher_button);
        assert tabSwitcherButton != null;
        Rect tabSwitcherRect = new Rect();
        boolean tabSwitcherButtonIsVisible =
                tabSwitcherButton.getGlobalVisibleRect(tabSwitcherRect);

        Context context = getContext();
        mBackgroundHostView =
                (NewBackgroundTabAnimationHostView)
                        LayoutInflater.from(context)
                                .inflate(
                                        R.layout.new_background_tab_animation_host_view,
                                        mAnimationHostView,
                                        false);
        assumeNonNull(mTabModelSelector);
        int prevTabCount = mTabModelSelector.getModel(isIncognito).getCount() - 1;
        mCustomTabCountToken = mCustomTabCount.setCount(prevTabCount);

        @ColorInt
        int toolbarColor =
                isRegularNtp
                        ? NewTabAnimationUtils.getBackgroundColor(context, isIncognito)
                        : mToolbarManager.getPrimaryColor();
        int[] toolbarPosition = new int[2];
        mAnimationHostView.findViewById(R.id.toolbar).getLocationInWindow(toolbarPosition);
        boolean isTopToolbar =
                isRegularNtp || ToolbarPositionController.shouldShowToolbarOnTop(animationTab);
        int toolbarHeight = toolbarPosition[1] + getTopInsetIfNeeded(animationTab);

        Rect compositorViewRect = new Rect();
        mCompositorViewHolder.getGlobalVisibleRect(compositorViewRect);

        ObservableSupplier<Float> ntpSearchBoxTransitionPercentageSupplier =
                mToolbarManager.getNtpSearchBoxTransitionPercentageSupplier();

        @AnimationType
        int animationType =
                NewBackgroundTabAnimationHostView.calculateAnimationType(
                        tabSwitcherButtonIsVisible,
                        isRegularNtp,
                        ntpSearchBoxTransitionPercentageSupplier.get());

        @BrandedColorScheme int brandedColorScheme;
        if (isRegularNtp
                && animationType == AnimationType.DEFAULT
                && NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false)) {
            brandedColorScheme = BrandedColorScheme.DARK_BRANDED_THEME;
        } else {
            brandedColorScheme =
                    ThemeUtils.getBrandedColorScheme(context, toolbarColor, isIncognito);
        }

        if (mTopInsetCoordinator != null && animationType == AnimationType.DEFAULT) {
            mTabSwitcherButton = tabSwitcherButton;
            toolbarColor = Color.TRANSPARENT;
        }

        mBackgroundHostView.setUpAnimation(
                tabSwitcherButton,
                tabSwitcherRect,
                isIncognito,
                isTopToolbar,
                toolbarColor,
                animationType,
                brandedColorScheme,
                prevTabCount,
                toolbarHeight,
                compositorViewRect.top,
                compositorViewRect.left);

        // {@link View#INVISIBLE} is needed to generate the geometry information.
        mBackgroundHostView.setVisibility(View.INVISIBLE);

        // It makes sure to add the view under the message container so the inner container in
        // NewBackgroundTabFakeTabSwitcherButton does not clash with the message view (Ex:
        // Translate).
        ViewGroup messageContainer = mAnimationHostView.findViewById(R.id.message_container);
        if (messageContainer != null) {
            int index = mAnimationHostView.indexOfChild(messageContainer);
            mAnimationHostView.addView(mBackgroundHostView, index);
        } else {
            mAnimationHostView.addView(mBackgroundHostView);
        }
        // This ensures the view to be properly laid out in order to do calculations within the
        // background animation host view. The main reason we need this is to get values from
        // {@link NewBackgroundTabSwitcherButton#getButtonLocation}.
        mAnimationRunnable =
                () -> {
                    mAnimationRunnable = null;
                    mTimeoutRunnable = null;
                    assumeNonNull(mTabModelSelector);
                    assumeNonNull(mBackgroundHostView);
                    boolean shouldObserveNtp =
                            isRegularNtp && animationType == AnimationType.DEFAULT;
                    AnimationInterruptor interruptor =
                            new AnimationInterruptor(
                                    mLayoutStateProvider,
                                    mTabModelSelector.getCurrentTabSupplier(),
                                    animationTab,
                                    mScrimVisibilitySupplier,
                                    ntpSearchBoxTransitionPercentageSupplier,
                                    shouldObserveNtp,
                                    this::forceAnimationToFinish);
                    assumeNonNull(mBackgroundHostView);
                    mTabCreatedBackgroundAnimation = mBackgroundHostView.getAnimatorSet(x, y);
                    AnimationFreezeChecker checker =
                            new AnimationFreezeChecker(AnimationFreezeChecker.BACKGROUND_TAG);
                    mTabCreatedBackgroundAnimation.addListener(
                            new CancelAwareAnimatorListener() {
                                private void internalBackgroundCleanUp() {
                                    interruptor.destroy();
                                    cleanUpBackgroundAnimation();
                                }

                                @Override
                                public void onStart(Animator animation) {
                                    checker.onAnimationStart();

                                    if (mTabSwitcherButton != null) {
                                        mTabSwitcherButton.setVisibility(View.INVISIBLE);
                                    }
                                    // Release custom tab count as soon as the animation starts to
                                    // avoid showing the old tab count if the user decides to scroll
                                    // up during AnimationType.NTP_PARTIAL_SCROLL or
                                    // AnimationType.NTP_FULL_SCROLL.
                                    mCustomTabCount.releaseCount(mCustomTabCountToken);
                                    mCustomTabCountToken = TokenHolder.INVALID_TOKEN;
                                }

                                @Override
                                public void onEnd(Animator animation) {
                                    checker.onAnimationEnd();
                                    internalBackgroundCleanUp();
                                }

                                @Override
                                public void onCancel(Animator animation) {
                                    checker.onAnimationCancel();
                                    internalBackgroundCleanUp();
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
                    cleanUpBackgroundAnimation();
                    mCustomTabCount.releaseCount(mCustomTabCountToken);
                    mCustomTabCountToken = TokenHolder.INVALID_TOKEN;
                    if (mVisibilityObserver != null) {
                        visibilitySupplier.removeObserver(mVisibilityObserver);
                        mVisibilityObserver = null;
                    }
                };

        mVisibilityObserver =
                visible -> {
                    if (!visible) {
                        assert mTimeoutRunnable != null;
                        mHandler.removeCallbacks(mTimeoutRunnable);
                        mTimeoutRunnable = null;
                        assert mAnimationRunnable != null;
                        assert mBackgroundHostView != null;
                        setRunOnNextLayout(mBackgroundHostView, mAnimationRunnable);
                        visibilitySupplier.removeObserver(assumeNonNull(mVisibilityObserver));
                        mVisibilityObserver = null;
                    }
                };

        if (visibilitySupplier.get()) {
            visibilitySupplier.addObserver(mVisibilityObserver);
            mHandler.postDelayed(mTimeoutRunnable, ANIMATION_TIMEOUT_MS);
        } else {
            setRunOnNextLayout(mBackgroundHostView, mAnimationRunnable);
        }
    }

    private void cleanUpBackgroundAnimation() {
        if (mTabSwitcherButton != null) {
            mTabSwitcherButton.setVisibility(View.VISIBLE);
            mTabSwitcherButton = null;
        }
        mTabCreatedBackgroundAnimation = null;
        mAnimationHostView.removeView(mBackgroundHostView);
        mBackgroundHostView = null;
        if (mBrowserControlsVisibilityToken != TokenHolder.INVALID_TOKEN) {
            mBrowserVisibilityDelegate.releasePersistentShowingToken(
                    mBrowserControlsVisibilityToken);
            mBrowserControlsVisibilityToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private int getTopInsetIfNeeded(@Nullable Tab tab) {
        if (mTopInsetCoordinator != null
                && NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(tab)) {
            return mTopInsetCoordinator.getSystemTopInset();
        }
        return 0;
    }

    private void forceHidingImmediatelyIfNeeded(boolean isNtp) {
        startHiding();
        if (mTopInsetCoordinator != null && isNtp) doneHiding();
    }

    private void onTopInsetCoordinatorAvailable(TopInsetCoordinator topInsetCoordinator) {
        mTopInsetCoordinatorSupplier.removeObserver(mTopInsetCoordinatorObserver);
        mTopInsetCoordinator = topInsetCoordinator;
    }

    protected void setRunOnNextLayoutImmediatelyForTesting(boolean runImmediately) {
        mRunOnNextLayoutImmediatelyForTesting = runImmediately;
    }

    protected void setNextTabIdForTesting(@TabId int nextTabId) {
        mNextTabId = nextTabId;
    }
}
