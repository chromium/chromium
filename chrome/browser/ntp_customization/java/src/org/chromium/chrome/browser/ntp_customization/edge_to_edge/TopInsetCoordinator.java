// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.edge_to_edge;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
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
    private final TabSupplierObserver mTabSupplierObserver;
    private final TabObserver mTabObserver;
    private final InsetObserver mInsetObserver;
    private final InsetObserver.WindowInsetsConsumer mWindowInsetsConsumer;
    private final NtpCustomizationConfigManager.HomepageStateListener mHomepageStateListener;

    private Insets mSystemInsets = Insets.NONE;
    private int mAppliedTopPadding;
    private boolean mConsumeTopInset;

    private @Nullable Tab mCurrentTab;

    /**
     * Instantiate the coordinator to handle drawing page into the Status bar area.
     *
     * @param tabObservableSupplier The supplier of current Tab instance.
     * @param insetObserver The {@link InsetObserver} that manages insets changes on the
     *     CoordinatorView.
     */
    public TopInsetCoordinator(
            ObservableSupplier<@Nullable Tab> tabObservableSupplier, InsetObserver insetObserver) {
        mInsetObserver = insetObserver;

        // Observing switches of Tabs.
        mTabSupplierObserver =
                new TabSupplierObserver(tabObservableSupplier) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        onTabSwitched(tab);
                    }
                };

        // Observing the events when 1) a Tab shows its native page or 2) a native page
        // navigates to a URL for web page.
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        if (tab != mCurrentTab) return;

                        // When a user opens a NTP, or navigates back to the NTP on the same tab,
                        // the TabSupplierObserver#onObservingDifferentTab() event isn't triggered.
                        // This is because the navigation occurs within the same tab. Instead, this
                        // scenario is handled by #onContentChanged(), which is called whenever a
                        // tab's content changes(to/from native pages or swapping native
                        // WebContents). Besides, we can't add a check on mCurrentTab similar to
                        // what #onTabSwitched() does. This is because when the NTP navigates to a
                        // new URL like foo.com, both the active tab and mCurrentTab will refer to
                        // foo.com. This makes it impossible to differentiate the navigation based
                        // solely on mCurrentTab.
                        mInsetObserver.retriggerOnApplyWindowInsets();
                    }
                };

        // Observing the NTP's background image changes. Showing edge to edge on top when a
        // customized image is set.
        mHomepageStateListener =
                new NtpCustomizationConfigManager.HomepageStateListener() {
                    @Override
                    public void onBackgroundChanged(@Nullable Drawable backgroundDrawable) {
                        mInsetObserver.retriggerOnApplyWindowInsets();
                    }
                };
        NtpCustomizationConfigManager.getInstance().addListener(mHomepageStateListener);

        mWindowInsetsConsumer = this::onApplyWindowInsets;
        mInsetObserver.addInsetsConsumer(
                mWindowInsetsConsumer, InsetConsumerSource.TOP_INSET_COORDINATOR);
        mInsetObserver.retriggerOnApplyWindowInsets();
    }

    // WindowInsetsConsumer implementation.
    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            View view, WindowInsetsCompat windowInsetsCompat) {
        if (mCurrentTab == null) return windowInsetsCompat;

        mSystemInsets = windowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars());

        // As long as the current native page supports to show edge to edge on top,
        // TopInsetCoordinator needs to consume the top padding every time when onApplyWindowInsets
        // is called to change the top padding of EdgeToEdgeLayout.
        mConsumeTopInset = NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(mCurrentTab);
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
        boolean isNtp = tab != null && UrlUtilities.isNtpUrl(tab.getUrl());
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
        }

        mCurrentTab = tab;
        if (mCurrentTab != null) {
            mCurrentTab.addObserver(mTabObserver);
        }

        boolean shouldReTriggerOnApplyWindowInsets = false;
        if (isNtp) {
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
        mTabSupplierObserver.destroy();
        mInsetObserver.removeInsetsConsumer(mWindowInsetsConsumer);
        if (mCurrentTab != null) {
            mCurrentTab.removeObserver(mTabObserver);
        }
        NtpCustomizationConfigManager.getInstance().removeListener(mHomepageStateListener);
    }

    /** Computes the new top Insets. */
    private void computeEdgePaddings() {
        // Computes the top padding to reflect whether ToEdge or ToNormal for the Status Bar.
        mAppliedTopPadding = mConsumeTopInset ? 0 : mSystemInsets.top;
    }

    public boolean getConsumeTopInsetForTesting() {
        return mConsumeTopInset;
    }

    public int getObserverCountForTesting() {
        return mObservers.size();
    }
}
