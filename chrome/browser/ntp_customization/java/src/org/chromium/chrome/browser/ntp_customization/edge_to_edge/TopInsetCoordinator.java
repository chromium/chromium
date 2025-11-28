// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.edge_to_edge;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.insets.InsetObserver;

@NullMarked
/** Class to consume top Insets to make supported native page (NTP) truly edge to edge. */
public class TopInsetCoordinator implements InsetObserver.WindowInsetsConsumer {
    /** Observer to notify when a change has been made in the top inset. */
    public interface Observer {
        /**
         * Notifies that a change has been made in the top inset and supplies the new inset.
         *
         * @param systemTopInset The system's top inset. This represents the height of the status
         *     bar, regardless of whether the page is drawing edge-to-edge.
         * @param consumeTopInset Whether the system's top inset will be removed.
         */
        void onToEdgeChange(int systemTopInset, boolean consumeTopInset);
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final ObservableSupplier<@Nullable Tab> mTabSupplier;
    private final TabObserver mTabObserver;
    private final LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;
    private final InsetObserver mInsetObserver;
    private final InsetObserver.WindowInsetsConsumer mWindowInsetsConsumer;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final NtpCustomizationConfigManager.HomepageStateListener mHomepageStateListener;

    private Insets mSystemInsets = Insets.NONE;
    private int mAppliedTopPadding;
    private boolean mConsumeTopInset;

    // A flag to indicate whether it is in the layout transition from the Tab switcher to a NTP.
    private boolean mInTabSwitcherToNtpTransition;
    // If true, there is an attempt to add a LayoutStateObserver when the LayoutStateProvider hasn't
    // been initialized yet.
    private boolean mAddLayoutStateObserverPending;
    // A flag to indicate whether the Tab switcher is showing.
    private boolean mIsTabSwitcherShowing;

    private @Nullable TabSupplierObserver mTabSupplierObserver;
    private @Nullable Tab mTrackingTab;
    private @Nullable LayoutStateProvider mLayoutStateProvider;

    /**
     * Instantiate the coordinator to handle drawing page into the Status bar area.
     *
     * @param context The Activity context.
     * @param tabSupplier The supplier of current Tab instance.
     * @param insetObserver The {@link InsetObserver} that manages insets changes on the
     *     CoordinatorView.
     * @param layoutStateProviderSupplier The supplier of {@link LayoutStateProvider}.
     */
    public TopInsetCoordinator(
            Context context,
            ObservableSupplier<@Nullable Tab> tabSupplier,
            InsetObserver insetObserver,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier) {
        mInsetObserver = insetObserver;
        mTabSupplier = tabSupplier;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;

        // Observing the events when 1) a Tab shows its native page or 2) a native page
        // navigates to a URL for web page. This observer is only added when needed.
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        if (tab != mTrackingTab) return;

                        // When a user opens a NTP, or navigates back to the NTP on the same tab,
                        // the TabSupplierObserver#onObservingDifferentTab() event isn't triggered.
                        // This is because the navigation occurs within the same tab. Instead, this
                        // scenario is handled by #onContentChanged(), which is called whenever a
                        // tab's content changes(to/from native pages or swapping native
                        // WebContents). Besides, we can't add a check on mTrackingTab similar to
                        // what #onTabSwitched() does. This is because when the NTP navigates to a
                        // new URL like foo.com, both the active tab and mTrackingTab will refer to
                        // foo.com. This makes it impossible to differentiate the navigation based
                        // solely on mTrackingTab.
                        mInsetObserver.retriggerOnApplyWindowInsets();
                    }
                };

        mLayoutStateObserver =
                new LayoutStateProvider.LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        // The mIsTabSwitcherShowing will be used to check if a transition between
                        // Tab switcher and NTP happens.
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            mIsTabSwitcherShowing = true;
                        } else {
                            mIsTabSwitcherShowing = false;
                        }

                        // When GTS is hiding, ToolbarPositionController updates the position of
                        // toolbar in #onFinishedShowing() of the next layout. Therefore, calling
                        // retriggerOnApplyWindowInsets() to apply the top insets if the transition
                        // happens from GTS to a NTP. This can't be handled in #onTabSwitched()
                        // which happens before the ToolbarPositionController updates the Toolbar's
                        // position.
                        if (mInTabSwitcherToNtpTransition && layoutType == LayoutType.BROWSING) {
                            mInTabSwitcherToNtpTransition = false;
                            mInsetObserver.retriggerOnApplyWindowInsets();
                        }
                    }
                };
        mLayoutStateProviderSupplier.onAvailable(this::onLayoutStateProviderAvailable);

        // Observing the NTP's background image changes. Showing edge to edge on top when a
        // customized image is set.
        mHomepageStateListener =
                new NtpCustomizationConfigManager.HomepageStateListener() {
                    @Override
                    public void onBackgroundImageChanged(
                            Bitmap originalBitmap,
                            @Nullable BackgroundImageInfo backgroundImageInfo,
                            boolean fromInitialization,
                            @NtpBackgroundImageType int oldType,
                            @NtpBackgroundImageType int newType) {
                        onNtpBackgroundChanged(fromInitialization, oldType, newType);
                    }

                    @Override
                    public void onBackgroundColorChanged(
                            @Nullable NtpThemeColorInfo ntpThemeColorInfo,
                            @ColorInt int backgroundColor,
                            boolean fromInitialization,
                            @NtpBackgroundImageType int oldType,
                            @NtpBackgroundImageType int newType) {
                        onNtpBackgroundChanged(fromInitialization, oldType, newType);
                    }

                    @Override
                    public void refreshWindowInsets(boolean consumeTopInset) {
                        TopInsetCoordinator.this.refreshWindowInsets(consumeTopInset);
                    }
                };
        NtpCustomizationConfigManager.getInstance().addListener(mHomepageStateListener, context);

        mWindowInsetsConsumer = this::onApplyWindowInsets;
        mInsetObserver.addInsetsConsumer(
                mWindowInsetsConsumer, InsetConsumerSource.TOP_INSET_COORDINATOR);
        mInsetObserver.retriggerOnApplyWindowInsets();
    }

    // WindowInsetsConsumer implementation.
    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            View view, WindowInsetsCompat windowInsetsCompat) {
        // We shouldn't use mTrackingTab, which can be set to null in removeObservers(), and it
        // won't be updated if mTabSupplierObserver is removed.
        Tab currentTab = mTabSupplier.get();
        if (currentTab == null) return windowInsetsCompat;

        mSystemInsets = windowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars());

        // As long as the current native page supports to show edge to edge on top,
        // TopInsetCoordinator needs to consume the top padding every time when onApplyWindowInsets
        // is called to change the top padding of EdgeToEdgeLayout.
        mConsumeTopInset = NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(currentTab);
        computeEdgePaddings();
        notifyObservers();

        if (!mConsumeTopInset) return windowInsetsCompat;

        var builder = new WindowInsetsCompat.Builder(windowInsetsCompat);
        // Consume top insets and display cutoff by forcing 0 as the top padding.
        if (mAppliedTopPadding == 0) {
            builder.setInsets(WindowInsetsCompat.Type.statusBars(), Insets.NONE);
            builder.setInsets(WindowInsetsCompat.Type.captionBar(), Insets.NONE);
            Insets displayCutout =
                    windowInsetsCompat.getInsets(WindowInsetsCompat.Type.displayCutout());
            if (displayCutout.top > 0) {
                builder.setInsets(WindowInsetsCompat.Type.displayCutout(), Insets.NONE);
            }
        }
        return builder.build();
    }

    /** Adds an observer. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Calls mInsetObserver.retriggerOnApplyWindowInsets() if the new Tab or the previous Tab
     * supports edge to edge on top.
     *
     * @param tab The new Tab to show.
     */
    @VisibleForTesting
    void onTabSwitched(@Nullable Tab tab) {
        // NTP is the only NativePage to support edge to edge on top so far. To reduce the times to
        // call retriggerOnApplyWindowInsets(), adds specific check of NTP URLs.
        boolean isRegularNtp =
                tab != null && !tab.isIncognito() && UrlUtilities.isNtpUrl(tab.getUrl());

        if (mIsTabSwitcherShowing && isRegularNtp) {
            mInTabSwitcherToNtpTransition = true;
        }

        if (mTrackingTab != null) {
            mTrackingTab.removeObserver(mTabObserver);
        }

        mTrackingTab = tab;

        if (mTrackingTab != null) {
            mTrackingTab.addObserver(mTabObserver);
        }

        // Holds off calling mInsetObserver.retriggerOnApplyWindowInsets() until the layout
        // transition ends. This could prevent removing the toolbar padding too early, i.e., the
        // GTS is fading out while NTP is still in the expanding animation.
        // mInsetObserver.retriggerOnApplyWindowInsets() will be called in
        // LayoutStateObserver#onFinishedShowing().
        if (mInTabSwitcherToNtpTransition) return;

        boolean shouldReTriggerOnApplyWindowInsets = false;
        if (isRegularNtp) {
            // In case the NewTabPage hasn't be created when onObservingDifferentTab() is called,
            // don't call retriggerOnApplyWindowInsets(). It will be called in
            // mTabObserver#onContentChanged().
            if (tab != null && tab.isNativePage()) {
                shouldReTriggerOnApplyWindowInsets = true;
            }
        } else if (mConsumeTopInset) {
            // If the previous Tab supports to draw edge to edge on top while the new Tab doesn't,
            // we need to trigger an update of the top padding of the root view.
            shouldReTriggerOnApplyWindowInsets = true;
        }

        if (shouldReTriggerOnApplyWindowInsets) {
            mInsetObserver.retriggerOnApplyWindowInsets();
        }
    }

    private void notifyObservers() {
        for (var observer : mObservers) {
            observer.onToEdgeChange(mSystemInsets.top, mConsumeTopInset);
        }
    }

    /** Destroys the TopInsetCoordinator instance. */
    public void destroy() {
        mObservers.clear();
        removeObservers();
        mInsetObserver.removeInsetsConsumer(mWindowInsetsConsumer);
        NtpCustomizationConfigManager.getInstance().removeListener(mHomepageStateListener);
    }

    // Called when a customized background of NTP is selected or removed. It initializes or removes
    // observers which track the Tab and Layout transitions.
    @VisibleForTesting
    void onNtpBackgroundChanged(
            boolean fromInitialization,
            @NtpBackgroundImageType int oldType,
            @NtpBackgroundImageType int newType) {
        boolean shouldRefreshWindowInsets = false;
        if (oldType != newType && newType == NtpBackgroundImageType.DEFAULT) {
            removeObservers();
            shouldRefreshWindowInsets = true;
        } else if (oldType != newType && oldType == NtpBackgroundImageType.DEFAULT) {
            addObservers();
            shouldRefreshWindowInsets = true;
        }

        if (fromInitialization || !shouldRefreshWindowInsets) return;

        refreshWindowInsets(newType != NtpBackgroundImageType.DEFAULT);
    }

    /** Returns the system's top inset. */
    public int getSystemTopInset() {
        return mSystemInsets.top;
    }

    // Adds observers which track Tab and Layout transitions and are only needed when the customized
    // background is selected for NTPs.
    private void addObservers() {
        // Observing switches of Tabs.
        if (mTabSupplierObserver == null) {
            mTabSupplierObserver =
                    new TabSupplierObserver(mTabSupplier) {
                        @Override
                        protected void onObservingDifferentTab(@Nullable Tab tab) {
                            onTabSwitched(tab);
                        }
                    };
        }

        if (mTrackingTab == null) {
            mTrackingTab = mTabSupplier.get();
            if (mTrackingTab != null) {
                mTrackingTab.addObserver(mTabObserver);
            }
        }

        if (mLayoutStateProvider == null) {
            mLayoutStateProvider = mLayoutStateProviderSupplier.get();
        }
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.addObserver(mLayoutStateObserver);
        } else {
            mAddLayoutStateObserverPending = true;
        }
    }

    // Removes observers which track Tab and Layout transitions and are only needed when the
    // customized background is selected for NTPs.
    private void removeObservers() {
        if (mTabSupplierObserver != null) {
            mTabSupplierObserver.destroy();
            mTabSupplierObserver = null;
        }

        if (mTrackingTab != null) {
            mTrackingTab.removeObserver(mTabObserver);
            mTrackingTab = null;
        }

        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
            mLayoutStateProvider = null;
        }
        mAddLayoutStateObserverPending = false;
    }

    /** Called when the {@link LayoutStateProvider} is available. */
    private void onLayoutStateProviderAvailable(LayoutStateProvider layoutStateProvider) {
        if (!mAddLayoutStateObserverPending) return;

        assert mLayoutStateProvider == null;
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
        mAddLayoutStateObserverPending = false;
    }

    /** Computes the new top Insets. */
    private void computeEdgePaddings() {
        // Computes the top padding to reflect whether ToEdge or ToNormal for the Status Bar.
        mAppliedTopPadding = mConsumeTopInset ? 0 : mSystemInsets.top;
    }

    private void refreshWindowInsets(boolean consumeTopInset) {
        mConsumeTopInset = consumeTopInset;
        mInsetObserver.retriggerOnApplyWindowInsets();
    }

    public boolean getConsumeTopInsetForTesting() {
        return mConsumeTopInset;
    }

    public int getObserverCountForTesting() {
        return mObservers.size();
    }

    public @Nullable TabSupplierObserver getTabSupplierObserverForTesting() {
        return mTabSupplierObserver;
    }

    public @Nullable Tab getTrackingTabForTesting() {
        return mTrackingTab;
    }

    public boolean getAddLayoutStateObserverPendingForTesting() {
        return mAddLayoutStateObserverPending;
    }

    public boolean getIsTabSwitcherShowingForTesting() {
        return mIsTabSwitcherShowing;
    }

    public boolean getInTabSwitcherToNtpTransitionForTesting() {
        return mInTabSwitcherToNtpTransition;
    }
}
