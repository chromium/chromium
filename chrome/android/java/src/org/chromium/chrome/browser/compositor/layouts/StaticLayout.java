// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.RectF;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
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
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.function.Supplier;

// TODO(meiliang): Rename to StaticLayoutMediator.
/**
 * A {@link Layout} that shows a single tab at full screen. This tab is chosen based on the {@link
 * #tabSelecting(long, int)} call, and is used to show a thumbnail of a {@link Tab} until that
 * {@link Tab} is ready to be shown.
 */
@NullMarked
public class StaticLayout extends Layout {
    public static final String TAG = "StaticLayout";

    private static @Nullable Integer sToolbarTextBoxBackgroundColorForTesting;

    private final boolean mHandlesTabLifecycles;
    private final boolean mNeedsOffsetTag;

    private final Context mContext;
    private final LayoutManagerHost mViewHost;
    private final CompositorModelChangeProcessor.FrameRequestSupplier mRequestSupplier;

    private final PropertyModel mModel;
    private CompositorModelChangeProcessor mMcp;

    private StaticTabSceneLayer mSceneLayer;

    private @Nullable TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private @Nullable TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final BrowserControlsStateProvider.Observer mBrowserControlsStateProviderObserver;

    private final Supplier<TopUiThemeColorProvider> mTopUiThemeColorProvider;

    private boolean mIsShowing;

    @SuppressWarnings("HidingField")
    private final float mPxToDp;

    /**
     * Creates an instance of the {@link StaticLayout}.
     *
     * @param context The current Android's context.
     * @param updateHost The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost The {@link LayoutRenderHost} view for this layout.
     * @param viewHost The {@link LayoutManagerHost} view for this layout.
     * @param requestSupplier Frame request supplier for Compositor MCP.
     * @param tabModelSelector {@link TabModelSelector} instance.
     * @param tabContentManager {@link TabContentsManager} instance.
     * @param browserControlsStateProvider A {@link BrowserControlsStateProvider}.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param needsOffsetTag Whether or not this layout needs an OffsetTag.
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
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            boolean needsOffsetTag) {
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
                null,
                needsOffsetTag);
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
            @Nullable StaticTabSceneLayer testSceneLayer,
            boolean needsOffsetTag) {
        super(context, updateHost, renderHost);

        mContext = context;

        // Only handle tab lifecycle on tablets.
        mHandlesTabLifecycles = DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext);

        // StaticTabSceneLayer is a subtree of TabStripSceneLayer, and the tag would have been set
        // on the TabStripSceneLayer already if tablet UI is present.
        mNeedsOffsetTag = needsOffsetTag;

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
                        .with(LayoutTab.IS_ACTIVE_LAYOUT, false)
                        .build();

        mTopUiThemeColorProvider = topUiThemeColorProvider;

        Resources res = context.getResources();
        float dpToPx = res.getDisplayMetrics().density;
        mPxToDp = 1.0f / dpToPx;

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mModel.set(LayoutTab.CONTENT_OFFSET, mBrowserControlsStateProvider.getContentOffset());
        mBrowserControlsStateProviderObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onOffsetTagsInfoChanged(
                            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                            BrowserControlsOffsetTagsInfo offsetTagsInfo,
                            @BrowserControlsState int constraints,
                            boolean shouldUpdateOffsets) {
                        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
                            if (mNeedsOffsetTag) {
                                mModel.set(
                                        LayoutTab.CONTENT_OFFSET_TAG,
                                        offsetTagsInfo.getContentOffsetTag());
                            }

                            if (shouldUpdateOffsets) {
                                mModel.set(
                                        LayoutTab.CONTENT_OFFSET,
                                        mBrowserControlsStateProvider.getContentOffset());
                            }
                        }
                    }

                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            boolean topControlsMinHeightChanged,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean bottomControlsMinHeightChanged,
                            boolean requestNewFrame,
                            boolean isVisibilityForced) {
                        if (!ChromeFeatureList.sBrowserControlsInViz.isEnabled()
                                || requestNewFrame
                                || isVisibilityForced) {
                            int contentOffset = mBrowserControlsStateProvider.getContentOffset();
                            mModel.set(LayoutTab.CONTENT_OFFSET, contentOffset);
                        } else {
                            // We need to set the height, as it would have changed if this is the
                            // first frame of an animation. Any existing offsets from scrolling and
                            // animations will be applied by OffsetTags.
                            int height = mBrowserControlsStateProvider.getTopControlsHeight();
                            mModel.set(LayoutTab.CONTENT_OFFSET, height);
                        }
                    }
                };
        mBrowserControlsStateProvider.addObserver(mBrowserControlsStateProviderObserver);

        if (testSceneLayer != null) {
            mSceneLayer = testSceneLayer;
        } else {
            mSceneLayer = new StaticTabSceneLayer();
        }
        assumeNonNull(mTabContentManager);
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
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        if (!mIsShowing) return;

                        setStaticTab(tab);
                        requestFocus(tab);
                    }
                };

        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(tabModelSelector) {

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
                    public void didBackForwardTransitionAnimationChange(Tab tab) {
                        updateStaticTab(tab, /* skipUpdateVisibleIds= */ false);
                    }
                };
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.DYNAMIC_BROWSER_CONTROLS;
    }

    /**
     * Initialize the layout to be shown.
     *
     * @param time The current time of the app in ms.
     * @param animate Whether to play an entry animation.
     */
    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        mIsShowing = true;
        assumeNonNull(mTabModelSelector);
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
        assumeNonNull(mTabModelSelector);
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab == null) return;
        requestFocus(tab);
    }

    @Override
    public void doneHiding() {
        mIsShowing = false;
        mModel.set(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID);

        // Call super last because it might re-show this layout. If we do any work after
        // super.doneHiding() the layout might become unexpectedly inactive or have an
        // incorrect tab id. See crbug/1468214.
        super.doneHiding();
    }

    private void requestFocus(Tab tab) {
        // We will restrict avoidance of tab focus request only on tablet devices, since this is
        // known to cause regressions on phones - see https://crbug.com/40069240 for details.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            return;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.REMOVE_TAB_FOCUS_ON_SHOWING_AND_SELECT)) {
            return;
        }

        if (mIsShowing && tab.getView() != null) tab.getView().requestFocus();
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

    private void setStaticTab(Tab tab) {
        assert tab != null;

        updateVisibleIdsCheckingLiveLayer(tab.getId(), canUseLiveTexture(tab));
        if (mModel.get(LayoutTab.TAB_ID) == tab.getId()) return;

        mModel.set(LayoutTab.TAB_ID, tab.getId());
        mModel.set(LayoutTab.IS_INCOGNITO, tab.isIncognito());
        mModel.set(LayoutTab.ORIGINAL_CONTENT_WIDTH_IN_DP, mViewHost.getWidth() * mPxToDp);
        mModel.set(LayoutTab.ORIGINAL_CONTENT_HEIGHT_IN_DP, mViewHost.getHeight() * mPxToDp);
        mModel.set(LayoutTab.MAX_CONTENT_WIDTH, mViewHost.getWidth() * mPxToDp);
        mModel.set(LayoutTab.MAX_CONTENT_HEIGHT, mViewHost.getHeight() * mPxToDp);

        updateStaticTab(tab, /* skipUpdateVisibleIds= */ true);
    }

    private void updateStaticTab(Tab tab, boolean skipUpdateVisibleIds) {
        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) return;

        boolean useLiveTexture = canUseLiveTexture(tab);
        if (!skipUpdateVisibleIds) {
            updateVisibleIdsCheckingLiveLayer(tab.getId(), useLiveTexture);
        }

        TopUiThemeColorProvider topUiTheme = mTopUiThemeColorProvider.get();
        mModel.set(LayoutTab.BACKGROUND_COLOR, topUiTheme.getBackgroundColor(tab));
        mModel.set(LayoutTab.TOOLBAR_BACKGROUND_COLOR, topUiTheme.getSceneLayerBackground(tab));
        mModel.set(LayoutTab.TEXT_BOX_BACKGROUND_COLOR, getToolbarTextBoxBackgroundColor(tab));
        mModel.set(LayoutTab.CAN_USE_LIVE_TEXTURE, useLiveTexture);
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

    private boolean canUseLiveTexture(Tab tab) {
        final WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;

        final GURL url = tab.getUrl();
        final boolean isNativePage =
                tab.isNativePage() || url.getScheme().equals(UrlConstants.CHROME_NATIVE_SCHEME);
        final boolean isBFScreenshotDrawing =
                isNativePage && tab.isDisplayingBackForwardAnimation();
        return !SadTab.isShowing(tab) && (!isNativePage || isBFScreenshotDrawing);
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
    protected @Nullable EventFilter getEventFilter() {
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
    protected void setIsActive(boolean active) {
        super.setIsActive(active);
        mModel.set(LayoutTab.IS_ACTIVE_LAYOUT, active);
    }

    @Override
    @SuppressWarnings("NullAway")
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

    @Nullable TabModelSelector getTabModelSelectorForTesting() {
        return mTabModelSelector;
    }

    @Nullable TabContentManager getTabContentManagerForTesting() {
        return mTabContentManager;
    }

    BrowserControlsStateProvider getBrowserControlsStateProviderForTesting() {
        return mBrowserControlsStateProvider;
    }

    public int getCurrentTabIdForTesting() {
        return mModel.get(LayoutTab.TAB_ID);
    }
}
