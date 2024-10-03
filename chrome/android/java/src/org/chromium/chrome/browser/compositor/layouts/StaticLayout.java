// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.animation.Animator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.RectF;
import android.os.Handler;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsOffsetTagsInfo;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarFeatures;
import org.chromium.components.browser_ui.widget.animation.CancelAwareAnimatorListener;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.url.GURL;

import java.util.Collections;

// TODO(meiliang): Rename to StaticLayoutMediator.
/**
 * A {@link Layout} that shows a single tab at full screen. This tab is chosen based on the {@link
 * #tabSelecting(long, int)} call, and is used to show a thumbnail of a {@link Tab} until that
 * {@link Tab} is ready to be shown.
 */
public class StaticLayout extends Layout {
    public static final String TAG = "StaticLayout";

    private static final int HIDE_TIMEOUT_MS = 2000;
    private static final int HIDE_DURATION_MS = 500;

    private boolean mHandlesTabLifecycles;
    private boolean mNeedsOffsetTag;

    private class UnstallRunnable implements Runnable {
        @Override
        public void run() {
            mUnstalling = false;
            CompositorAnimator.ofWritableFloatPropertyKey(
                            mAnimationHandler,
                            mModel,
                            LayoutTab.SATURATION,
                            mModel.get(LayoutTab.SATURATION),
                            1.0f,
                            HIDE_DURATION_MS)
                    .start();
            CompositorAnimator animator =
                    CompositorAnimator.ofWritableFloatPropertyKey(
                            mAnimationHandler,
                            mModel,
                            LayoutTab.STATIC_TO_VIEW_BLEND,
                            mModel.get(LayoutTab.STATIC_TO_VIEW_BLEND),
                            0.0f,
                            HIDE_DURATION_MS);
            animator.addListener(
                    new CancelAwareAnimatorListener() {
                        @Override
                        public void onEnd(Animator animation) {
                            updateVisibleIdsCheckingLiveLayer(
                                    mModel.get(LayoutTab.TAB_ID),
                                    mModel.get(LayoutTab.CAN_USE_LIVE_TEXTURE));
                        }
                    });
            animator.start();
            mModel.set(LayoutTab.SHOULD_STALL, false);
        }
    }

    private final Context mContext;
    private final LayoutManagerHost mViewHost;
    private final CompositorModelChangeProcessor.FrameRequestSupplier mRequestSupplier;

    private final PropertyModel mModel;
    private CompositorModelChangeProcessor mMcp;

    private StaticTabSceneLayer mSceneLayer;

    private final UnstallRunnable mUnstallRunnable;
    private final Handler mHandler;
    private boolean mUnstalling;

    private TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    private BrowserControlsStateProvider.Observer mBrowserControlsStateProviderObserver;

    private final CompositorAnimationHandler mAnimationHandler;
    private final Supplier<TopUiThemeColorProvider> mTopUiThemeColorProvider;

    private boolean mIsActive;

    private static Integer sToolbarTextBoxBackgroundColorForTesting;

    private float mPxToDp;

    /**
     * Creates an instance of the {@link StaticLayout}.
     * @param context             The current Android's context.
     * @param updateHost          The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost          The {@link LayoutRenderHost} view for this layout.
     * @param viewHost            The {@link LayoutManagerHost} view for this layout
     * @param requestSupplier Frame request supplier for Compositor MCP.
     * @param tabModelSelector {@link TabModelSelector} instance.
     * @param tabContentManager {@link TabContentsManager} instance.
     * @param browserControlsStateProvider A {@link BrowserControlsStateProvider}.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public StaticLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            LayoutManagerHost viewHost,
            CompositorModelChangeProcessor.FrameRequestSupplier requestSupplier,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider) {
        this(
                context,
                updateHost,
                renderHost,
                viewHost,
                requestSupplier,
                tabModelSelector,
                tabContentManager,
                browserControlsStateProvider,
                topUiThemeColorProvider,
                null);
    }

    /** Protected constructor for testing, allows specifying a custom SceneLayer. */
    @VisibleForTesting
    StaticLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            LayoutManagerHost viewHost,
            CompositorModelChangeProcessor.FrameRequestSupplier requestSupplier,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            StaticTabSceneLayer testSceneLayer) {
        super(context, updateHost, renderHost);

        mContext = context;
        boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);
        // Only handle tab lifecycle on tablets.
        mHandlesTabLifecycles = isTablet;
        // On tablets, StaticTabSceneLayer is a subtree of TabStripSceneLayer,
        // and the tag would have been set on the TabStripSceneLayer already.
        mNeedsOffsetTag = !isTablet;

        mViewHost = viewHost;
        mRequestSupplier = requestSupplier;

        setTabContentManager(tabContentManager);
        setTabModelSelector(tabModelSelector);

        mModel =
                new PropertyModel.Builder(LayoutTab.ALL_KEYS)
                        .with(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID)
                        .with(LayoutTab.SCALE, 1.0f)
                        .with(LayoutTab.X, 0.0f)
                        .with(LayoutTab.Y, 0.0f)
                        .with(LayoutTab.RENDER_X, 0.0f)
                        .with(LayoutTab.RENDER_Y, 0.0f)
                        .with(LayoutTab.SATURATION, 1.0f)
                        .with(LayoutTab.STATIC_TO_VIEW_BLEND, 0.0f)
                        .with(LayoutTab.IS_ACTIVE_LAYOUT_SUPPLIER, this::isActive)
                        .build();

        mAnimationHandler = updateHost.getAnimationHandler();
        mTopUiThemeColorProvider = topUiThemeColorProvider;

        mHandler = new Handler();
        mUnstallRunnable = new UnstallRunnable();
        mUnstalling = false;

        Resources res = context.getResources();
        float dpToPx = res.getDisplayMetrics().density;
        mPxToDp = 1.0f / dpToPx;

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mModel.set(LayoutTab.CONTENT_OFFSET, mBrowserControlsStateProvider.getContentOffset());
        mBrowserControlsStateProviderObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsConstraintsChanged(
                            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                            BrowserControlsOffsetTagsInfo offsetTagsInfo,
                            @BrowserControlsState int constraints) {
                        if (ToolbarFeatures.isBrowserControlsInVizEnabled(
                                DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext))) {
                            // On tablets, StaticTabSceneLayer is a subtree of TabStripSceneLayer,
                            // and the tag would have been set on the TabStripSceneLayer already.
                            if (mNeedsOffsetTag) {
                                mModel.set(
                                        LayoutTab.CONTENT_OFFSET_TAG,
                                        offsetTagsInfo.getTopControlsOffsetTag());
                            }

                            // With BCIV enabled, scrolling will not update the content offset of
                            // the browser's compositor frame. If we transition to a HIDDEN state
                            // while the controls are already scrolled offscreen, then there is no
                            // need to move the top controls, which means the renderer will not
                            // notify the browser to move them. We set the content offset here so
                            // the browser will submit a compositor frame with the correct offset.
                            int contentOffset = mBrowserControlsStateProvider.getContentOffset();
                            if (constraints == BrowserControlsState.HIDDEN
                                    && contentOffset
                                            == mBrowserControlsStateProvider
                                                    .getTopControlsMinHeight()) {
                                mModel.set(LayoutTab.CONTENT_OFFSET, contentOffset);
                            }
                        }
                    }

                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate,
                            boolean isVisibilityForced) {
                        if (!ToolbarFeatures.isBrowserControlsInVizEnabled(
                                        DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext))
                                || needsAnimate
                                || isVisibilityForced) {
                            mModel.set(
                                    LayoutTab.CONTENT_OFFSET,
                                    mBrowserControlsStateProvider.getContentOffset());
                        }
                    }
                };
        mBrowserControlsStateProvider.addObserver(mBrowserControlsStateProviderObserver);

        if (testSceneLayer != null) {
            mSceneLayer = testSceneLayer;
        } else {
            mSceneLayer = new StaticTabSceneLayer();
        }
        mSceneLayer.setTabContentManager(mTabContentManager);

        mMcp =
                CompositorModelChangeProcessor.create(
                        mModel, mSceneLayer, StaticTabSceneLayer::bind, mRequestSupplier);
    }

    @Override
    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        assert tabModelSelector != null;
        assert mTabModelSelector == null : "The TabModelSelector should set at most once";
        super.setTabModelSelector(tabModelSelector);

        // TODO(crbug.com/40126259): Investigating to use ActivityTabProvider instead.
        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(tabModelSelector) {
                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        if (!mIsActive) return;

                        setStaticTab(tab);
                        requestFocus(tab);
                    }
                };

        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(tabModelSelector) {
                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        if (mIsActive) unstallImmediately(tab.getId());
                    }

                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) {
                            setStaticTab(tab);
                        } else {
                            updateStaticTab(tab, /* skipUpdateVisibleIds= */ false);
                        }
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) return;

                        mModel.set(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID);
                    }

                    @Override
                    public void onTabUnregistered(Tab tab) {
                        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) return;

                        mModel.set(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID);
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        updateStaticTab(tab, /* skipUpdateVisibleIds= */ false);
                    }

                    @Override
                    public void onBackgroundColorChanged(Tab tab, int color) {
                        updateStaticTab(tab, /* skipUpdateVisibleIds= */ false);
                    }

                    @Override
                    public void onDidChangeThemeColor(Tab tab, int color) {
                        updateStaticTab(tab, /* skipUpdateVisibleIds= */ false);
                    }

                    @Override
                    public void didBackForwardTransitionAnimationChange() {
                        updateStaticTab(
                                tabModelSelector.getCurrentTab(),
                                /* skipUpdateVisibleIds= */ false);
                    }
                };
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.DYNAMIC_BROWSER_CONTROLS;
    }

    /**
     * Initialize the layout to be shown.
     * @param time   The current time of the app in ms.
     * @param animate Whether to play an entry animation.
     */
    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        mIsActive = true;
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab == null) return;
        setStaticTab(tab);
    }

    @Override
    protected void updateLayout(long time, long dt) {
        super.updateLayout(time, dt);
        updateSnap(dt, mModel);
    }

    @Override
    public void doneShowing() {
        super.doneShowing();
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab == null) return;
        requestFocus(tab);
    }

    @Override
    public void doneHiding() {
        mIsActive = false;
        mModel.set(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID);

        // Call super last because it might re-show this layout. If we do any work after
        // super.doneHiding() the layout might become unexpectedly inactive or have an
        // incorrect tab id. See crbug/1468214.
        super.doneHiding();
    }

    private void setPreHideState() {
        mHandler.removeCallbacks(mUnstallRunnable);
        mModel.set(LayoutTab.STATIC_TO_VIEW_BLEND, 1.0f);
        mModel.set(LayoutTab.SATURATION, 0.0f);
        mUnstalling = true;
    }

    private void setPostHideState() {
        mHandler.removeCallbacks(mUnstallRunnable);
        mModel.set(LayoutTab.STATIC_TO_VIEW_BLEND, 0.0f);
        mModel.set(LayoutTab.SATURATION, 1.0f);
        mUnstalling = false;
    }

    private void requestFocus(Tab tab) {
        // TODO(crbug.com/40249125): Investigating guarded removal of this behavior (requesting
        // focus on
        // a tab) since it may no longer be relevant.
        // We will restrict avoidance of tab focus request only on tablet devices, since this is
        // known to cause regressions on phones - see crbug.com/1471887 for details.
        if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AVOID_SELECTED_TAB_FOCUS_ON_LAYOUT_DONE_SHOWING)
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            return;
        }

        if (mIsActive && tab.getView() != null) tab.getView().requestFocus();
    }

    private void updateVisibleIdsCheckingLiveLayer(int tabId, boolean useLiveTexture) {
        // May be called when inactive. Prevent this from updating until the layout is shown.
        if (!isActive()) return;

        // Check if we can use the live texture as frozen or native pages don't support live layer.
        if (useLiveTexture) {
            updateCacheVisibleIdsAndPrimary(Collections.emptyList(), tabId);
        } else {
            updateCacheVisibleIdsAndPrimary(Collections.singletonList(tabId), tabId);
        }
    }

    private void updateVisibleIdsFromTab(Tab tab) {
        // May be called when inactive. Prevent this from updating until the layout is shown.
        if (!isActive()) return;

        final int tabId = tab.getId();
        if (shouldStall(tab)) {
            updateCacheVisibleIdsAndPrimary(Collections.singletonList(tabId), tabId);
        } else {
            updateVisibleIdsCheckingLiveLayer(tabId, canUseLiveTexture(tab));
        }
    }

    private void setStaticTab(Tab tab) {
        assert tab != null;

        if (mModel.get(LayoutTab.TAB_ID) == tab.getId() && !mModel.get(LayoutTab.SHOULD_STALL)) {
            updateVisibleIdsCheckingLiveLayer(tab.getId(), canUseLiveTexture(tab));
            setPostHideState();
            return;
        }

        updateVisibleIdsFromTab(tab);

        mModel.set(LayoutTab.TAB_ID, tab.getId());
        mModel.set(LayoutTab.IS_INCOGNITO, tab.isIncognito());
        mModel.set(LayoutTab.ORIGINAL_CONTENT_WIDTH_IN_DP, mViewHost.getWidth() * mPxToDp);
        mModel.set(LayoutTab.ORIGINAL_CONTENT_HEIGHT_IN_DP, mViewHost.getHeight() * mPxToDp);
        mModel.set(LayoutTab.MAX_CONTENT_WIDTH, mViewHost.getWidth() * mPxToDp);
        mModel.set(LayoutTab.MAX_CONTENT_HEIGHT, mViewHost.getHeight() * mPxToDp);

        updateStaticTab(tab, /* skipUpdateVisibleIds= */ true);

        if (mModel.get(LayoutTab.SHOULD_STALL)) {
            setPreHideState();
            mHandler.postDelayed(mUnstallRunnable, HIDE_TIMEOUT_MS);
        } else {
            setPostHideState();
        }
    }

    private void updateStaticTab(Tab tab, boolean skipUpdateVisibleIds) {
        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) return;

        if (!skipUpdateVisibleIds) {
            updateVisibleIdsFromTab(tab);
        }

        TopUiThemeColorProvider topUiTheme = mTopUiThemeColorProvider.get();
        mModel.set(LayoutTab.BACKGROUND_COLOR, topUiTheme.getBackgroundColor(tab));
        mModel.set(LayoutTab.TOOLBAR_BACKGROUND_COLOR, topUiTheme.getSceneLayerBackground(tab));
        mModel.set(LayoutTab.SHOULD_STALL, shouldStall(tab));
        mModel.set(LayoutTab.TEXT_BOX_BACKGROUND_COLOR, getToolbarTextBoxBackgroundColor(tab));
        mModel.set(LayoutTab.CAN_USE_LIVE_TEXTURE, canUseLiveTexture(tab));
    }

    private int getToolbarTextBoxBackgroundColor(Tab tab) {
        if (sToolbarTextBoxBackgroundColorForTesting != null) {
            return sToolbarTextBoxBackgroundColorForTesting;
        }

        return ThemeUtils.getTextBoxColorForToolbarBackground(
                mContext,
                tab,
                mTopUiThemeColorProvider.get().calculateColor(tab, tab.getThemeColor()));
    }

    void setTextBoxBackgroundColorForTesting(Integer color) {
        sToolbarTextBoxBackgroundColorForTesting = color;
    }

    // Whether the tab is ready to display or it should be faded in as it loads.
    private boolean shouldStall(Tab tab) {
        return (tab.isFrozen() || tab.needsReload()) && !tab.isNativePage();
    }

    private boolean canUseLiveTexture(Tab tab) {
        final WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;

        final GURL url = tab.getUrl();
        final boolean isNativePage =
                tab.isNativePage() || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME);
        final boolean isBFScreenshotDrawing =
                isNativePage && tab.isDisplayingBackForwardAnimation();
        assert !isBFScreenshotDrawing
                        || ChromeFeatureList.isEnabled(ChromeFeatureList.BACK_FORWARD_TRANSITIONS)
                : "Must not draw bf screenshot if back forward transition is disabled";
        return !SadTab.isShowing(tab) && (!isNativePage || isBFScreenshotDrawing);
    }

    @Override
    public void unstallImmediately(int tabId) {
        if (mModel.get(LayoutTab.TAB_ID) == tabId
                && mModel.get(LayoutTab.SHOULD_STALL)
                && mUnstalling) {
            unstallImmediately();
        }
    }

    @Override
    public void unstallImmediately() {
        if (mModel.get(LayoutTab.SHOULD_STALL) && mUnstalling) {
            mHandler.removeCallbacks(mUnstallRunnable);
            mUnstallRunnable.run();
        }
    }

    @Override
    public boolean handlesTabCreating() {
        return super.handlesTabCreating() || mHandlesTabLifecycles;
    }

    @Override
    public boolean handlesTabClosing() {
        return mHandlesTabLifecycles;
    }

    @Override
    public boolean shouldDisplayContentOverlay() {
        return true;
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    protected void updateSceneLayer(
            RectF viewport,
            RectF contentViewport,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        super.updateSceneLayer(
                viewport, contentViewport, tabContentManager, resourceManager, browserControls);
        assert mSceneLayer != null;
    }

    @Override
    public int getLayoutType() {
        return LayoutType.BROWSING;
    }

    @Override
    public void destroy() {
        if (mSceneLayer != null) {
            mSceneLayer.destroy();
            mSceneLayer = null;
        }
        if (mMcp != null) {
            mMcp.destroy();
            mMcp = null;
        }
        if (mTabModelSelector != null) {
            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabObserver.destroy();
        }
    }

    PropertyModel getModelForTesting() {
        return mModel;
    }

    TabModelSelector getTabModelSelectorForTesting() {
        return mTabModelSelector;
    }

    TabContentManager getTabContentManagerForTesting() {
        return mTabContentManager;
    }

    BrowserControlsStateProvider getBrowserControlsStateProviderForTesting() {
        return mBrowserControlsStateProvider;
    }

    public int getCurrentTabIdForTesting() {
        return mModel.get(LayoutTab.TAB_ID);
    }
}
