// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
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
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.Collections;

// TODO(meiliang): Rename to StaticLayoutMediator.
/**
 * A {@link Layout} that shows a single active {@link Tab} at full screen using its live layer (or
 * background color when a live layer is not available, such as for native or sad tabs).
 */
@NullMarked
public class StaticLayout extends Layout {
    public static final String TAG = "StaticLayout";

    // StaticTabSceneLayer is a subtree of TabStripSceneLayer, and the tag would have been set
    // on the TabStripSceneLayer already if tablet UI is present.
    private final NonNullObservableSupplier<Boolean> mNeedsOffsetTag;

    private final NonNullObservableSupplier<Long> mFrameRequestSupplier;
    private final Runnable mRequestFrameRunnable;

    private final PropertyModel mModel;
    private CompositorModelChangeProcessor mMcp;

    private StaticTabSceneLayer mSceneLayer;

    private @Nullable TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private @Nullable TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final BrowserControlsStateProvider.Observer mBrowserControlsStateProviderObserver;

    private boolean mIsShowing;
    private @Nullable BrowserControlsOffsetTagsInfo mOffsetTagsInfo;
    private final Callback<Boolean> mUpdateOffsetTagsCallback;

    /**
     * Creates an instance of the {@link StaticLayout}.
     *
     * @param context The current Android's context.
     * @param updateHost The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost The {@link LayoutRenderHost} view for this layout.
     * @param frameRequestSupplier Frame request supplier for Compositor MCP.
     * @param requestFrameRunnable Frame request runnable for Compositor MCP.
     * @param tabModelSelector {@link TabModelSelector} instance.
     * @param tabContentManager {@link TabContentManager} instance.
     * @param browserControlsStateProvider A {@link BrowserControlsStateProvider}.
     * @param needsOffsetTag Whether or not this layout needs an OffsetTag.
     */
    public StaticLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            NonNullObservableSupplier<Long> frameRequestSupplier,
            Runnable requestFrameRunnable,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            NonNullObservableSupplier<Boolean> needsOffsetTag) {
        this(
                context,
                updateHost,
                renderHost,
                frameRequestSupplier,
                requestFrameRunnable,
                tabModelSelector,
                tabContentManager,
                browserControlsStateProvider,
                null,
                needsOffsetTag);
    }

    /** Protected constructor for testing, allows specifying a custom SceneLayer. */
    @VisibleForTesting
    StaticLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            NonNullObservableSupplier<Long> frameRequestSupplier,
            Runnable requestFrameRunnable,
            TabModelSelector tabModelSelector,
            TabContentManager tabContentManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            @Nullable StaticTabSceneLayer testSceneLayer,
            NonNullObservableSupplier<Boolean> needsOffsetTag) {
        super(context, updateHost, renderHost);

        // StaticTabSceneLayer is a subtree of TabStripSceneLayer, and the tag would have been set
        // on the TabStripSceneLayer already if tablet UI is present.
        mNeedsOffsetTag = needsOffsetTag;

        mFrameRequestSupplier = frameRequestSupplier;
        mRequestFrameRunnable = requestFrameRunnable;

        setTabContentManager(tabContentManager);
        setTabModelSelector(tabModelSelector);

        mModel =
                new PropertyModel.Builder(LayoutTab.ALL_KEYS)
                        .with(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID)
                        .with(LayoutTab.BACKGROUND_COLOR, Color.TRANSPARENT)
                        .with(LayoutTab.RENDER_X, 0.0f)
                        .with(LayoutTab.RENDER_Y, 0.0f)
                        .with(LayoutTab.IS_ACTIVE_LAYOUT, false)
                        .build();

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mModel.set(LayoutTab.CONTENT_OFFSET_Y, mBrowserControlsStateProvider.getContentOffset());

        mUpdateOffsetTagsCallback = (ignored) -> updateOffsetTag();
        mNeedsOffsetTag.addSyncObserverAndPostIfNonNull(mUpdateOffsetTagsCallback);

        mBrowserControlsStateProviderObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onOffsetTagsInfoChanged(
                            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                            BrowserControlsOffsetTagsInfo offsetTagsInfo,
                            @BrowserControlsState int constraints,
                            boolean shouldUpdateOffsets) {
                        mOffsetTagsInfo = offsetTagsInfo;
                        updateOffsetTag();

                        if (shouldUpdateOffsets) {
                            mModel.set(
                                    LayoutTab.CONTENT_OFFSET_Y,
                                    mBrowserControlsStateProvider.getContentOffset());
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
                        if (requestNewFrame || isVisibilityForced) {
                            int contentOffset = mBrowserControlsStateProvider.getContentOffset();
                            mModel.set(LayoutTab.CONTENT_OFFSET_Y, contentOffset);
                        } else {
                            // We need to set the height, as it would have changed if this is the
                            // first frame of an animation. Any existing offsets from scrolling and
                            // animations will be applied by OffsetTags.
                            int height = mBrowserControlsStateProvider.getTopControlsHeight();
                            mModel.set(LayoutTab.CONTENT_OFFSET_Y, height);
                        }
                    }

                    @Override
                    public void onControlsPositionChanged(@ControlsPosition int controlsPosition) {
                        // An animated position change doesn't notify offsets until the renderer
                        // submits a frame at the new heights, but the browser frame carrying the
                        // new heights already embeds that renderer frame. Set the content offset
                        // to the new top height now, so the content offset tag is applied
                        // relative to the right base on that first frame.
                        // BrowserControlsManager#setControlsPosition only skips the synchronous
                        // offset notify for animated swaps on renderer-animated tabs. Native pages
                        // lock the controls to SHOWN (visibility forced), so the guard below
                        // leaves them to the offsets notified synchronously.
                        if (mBrowserControlsStateProvider
                                        .shouldAnimateBrowserControlsHeightChanges()
                                && !mBrowserControlsStateProvider.isVisibilityForced()) {
                            mModel.set(
                                    LayoutTab.CONTENT_OFFSET_Y,
                                    mBrowserControlsStateProvider.getTopControlsHeight());
                        }
                    }
                };
        mBrowserControlsStateProvider.addObserver(mBrowserControlsStateProviderObserver);

        if (testSceneLayer != null) {
            mSceneLayer = testSceneLayer;
        } else {
            mSceneLayer = new StaticTabSceneLayer(/* canUseThumbnail= */ false);
        }
        assumeNonNull(mTabContentManager);
        mSceneLayer.setTabContentManager(mTabContentManager);

        mMcp =
                CompositorModelChangeProcessor.create(
                        mModel,
                        mSceneLayer,
                        StaticTabSceneLayer::bind,
                        mFrameRequestSupplier,
                        mRequestFrameRunnable);
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
                    }
                };

        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(tabModelSelector) {

                    @Override
                    public void onShown(Tab tab, @TabSelectionType int type) {
                        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) {
                            setStaticTab(tab);
                        } else {
                            updateStaticTab(tab);
                        }
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) return;

                        mModel.set(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID);
                        mModel.set(LayoutTab.BACKGROUND_COLOR, Color.TRANSPARENT);
                    }

                    @Override
                    public void onTabUnregistered(Tab tab) {
                        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) return;

                        mModel.set(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID);
                        mModel.set(LayoutTab.BACKGROUND_COLOR, Color.TRANSPARENT);
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        updateStaticTab(tab);
                    }

                    @Override
                    public void onUrlUpdated(Tab tab) {
                        if (mModel.get(LayoutTab.TAB_ID) == tab.getId()
                                && canUseLiveTexture(tab)
                                        != mModel.get(LayoutTab.CAN_USE_LIVE_TEXTURE)) {
                            updateStaticTab(tab);
                        }
                    }

                    @Override
                    public void onBackgroundColorChanged(Tab tab, int color) {
                        updateStaticTab(tab);
                    }

                    @Override
                    public void didBackForwardTransitionAnimationChange(Tab tab) {
                        updateStaticTab(tab);
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
    public void doneHiding() {
        mIsShowing = false;
        mModel.set(LayoutTab.TAB_ID, Tab.INVALID_TAB_ID);
        mModel.set(LayoutTab.BACKGROUND_COLOR, Color.TRANSPARENT);

        // Call super last because it might re-show this layout. If we do any work after
        // super.doneHiding() the layout might become unexpectedly inactive or have an
        // incorrect tab id. See crbug.com/40068199.
        super.doneHiding();
    }

    private void updateVisibleIds(int tabId) {
        // May be called when inactive. Prevent this from updating until the layout is shown.
        if (!isActive()) return;

        updateCacheVisibleIdsAndPrimary(Collections.emptyList(), tabId);
    }

    private void setStaticTab(Tab tab) {
        assert tab != null;

        if (mModel.get(LayoutTab.TAB_ID) == tab.getId()) {
            updateVisibleIds(tab.getId());
            return;
        }

        mModel.set(LayoutTab.TAB_ID, tab.getId());
        updateStaticTab(tab);
    }

    private void updateStaticTab(Tab tab) {
        if (mModel.get(LayoutTab.TAB_ID) != tab.getId()) return;

        updateVisibleIds(tab.getId());
        mModel.set(LayoutTab.BACKGROUND_COLOR, ThemeUtils.getBackgroundColor(tab));
        mModel.set(LayoutTab.CAN_USE_LIVE_TEXTURE, canUseLiveTexture(tab));
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

    private void updateOffsetTag() {
        // LINT.IfChange(updateOffsetTag)
        var offsetTag =
                mNeedsOffsetTag.get() && mOffsetTagsInfo != null
                        ? mOffsetTagsInfo.getContentOffsetTag()
                        : null;

        if (offsetTag == mModel.get(LayoutTab.CONTENT_OFFSET_TAG)) return;

        mModel.set(LayoutTab.CONTENT_OFFSET_TAG, offsetTag);

        // When static layout has an offset tag update, we need to update the render immediately.
        // This is needed when tab strip hand-off the offset tag, the static layout has to remove
        // the current one before applying a new one. See crbug.com/472542453.
        if (offsetTag == null) {
            mSceneLayer.update(mModel);
        }
        // LINT.ThenChange()
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
    public int getLayoutType() {
        return LayoutType.BROWSING;
    }

    @Override
    protected void setIsActive(boolean active) {
        super.setIsActive(active);
        mModel.set(LayoutTab.IS_ACTIVE_LAYOUT, active);
    }

    /** Sets the {@link LayoutTab#CONTENT_OFFSET_X} for this layout. */
    protected void setContentOffsetX(@Px int contentOffsetX) {
        mModel.set(LayoutTab.CONTENT_OFFSET_X, contentOffsetX);
    }

    @Override
    @SuppressWarnings("NullAway")
    public void destroy() {
        mNeedsOffsetTag.removeObserver(mUpdateOffsetTagsCallback);
        mBrowserControlsStateProvider.removeObserver(mBrowserControlsStateProviderObserver);
        if (mSceneLayer != null) {
            mSceneLayer.destroy();
            mSceneLayer = null;
        }
        if (mMcp != null) {
            mMcp.destroy();
            mMcp = null;
        }
        if (mTabModelSelectorTabModelObserver != null) {
            mTabModelSelectorTabModelObserver.destroy();
            mTabModelSelectorTabModelObserver = null;
        }
        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
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
