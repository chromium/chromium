// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.text.TextPaint;
import android.text.style.ClickableSpan;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;
import org.chromium.url.GURL;

/** Mediator for the TabSharingToolbar. */
@NullMarked
class TabSharingToolbarMediator {
    // Span markers delimiting the clickable link substitutions in the status strings.
    private static final String LINK = "<link>";
    private static final String LINK_END = "</link>";
    private static final String LINK1 = "<link1>";
    private static final String LINK1_END = "</link1>";
    private static final String LINK2 = "<link2>";
    private static final String LINK2_END = "</link2>";

    private final Context mContext;
    private final PropertyModel mModel;
    private final TabSharingUiBridge mBridge;
    private final ActivityTabProvider mTabProvider;
    private final ActivityTabProvider.ActivityTabTabObserver mActiveTabObserver;

    private final WebContents mCapturer;
    private final WebContents mCapturee;
    private final CharSequence mCaptureeStatus;
    private final CharSequence mCapturerStatus;
    private final CharSequence mOtherTabsStatus;

    /**
     * Initializes the mediator.
     *
     * @param context The Android context.
     * @param model The {@link PropertyModel} for the toolbar.
     * @param bridge The bridge to the native tab sharing UI.
     * @param tabProvider The provider of the current tab.
     */
    public TabSharingToolbarMediator(
            Context context,
            PropertyModel model,
            TabSharingUiBridge bridge,
            ActivityTabProvider tabProvider) {
        mContext = context;
        mModel = model;
        mBridge = bridge;
        mTabProvider = tabProvider;

        mCapturer = mBridge.getCapturer();
        mCapturee = mBridge.getCapturee();

        ClickableSpan capturerSpan = buildClickToNavigateToTabSpan(mCapturer);
        ClickableSpan captureeSpan = buildClickToNavigateToTabSpan(mCapturee);
        String capturerName =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mCapturer.getLastCommittedUrl(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        String captureeName =
                UrlFormatter.formatUrlForSecurityDisplay(
                        mCapturee.getLastCommittedUrl(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        mCaptureeStatus =
                SpanApplier.applySpans(
                        mContext.getString(
                                R.string.tab_sharing_toolbar_sharing_current_tab_label,
                                LINK + capturerName + LINK_END),
                        new SpanInfo(LINK, LINK_END, capturerSpan));
        mCapturerStatus =
                SpanApplier.applySpans(
                        mContext.getString(
                                R.string.tab_sharing_toolbar_sharing_another_tab_to_this_tab_label,
                                LINK + captureeName + LINK_END),
                        new SpanInfo(LINK, LINK_END, captureeSpan));
        mOtherTabsStatus =
                SpanApplier.applySpans(
                        mContext.getString(
                                R.string.tab_sharing_toolbar_sharing_another_tab_label,
                                LINK1 + captureeName + LINK1_END,
                                LINK2 + capturerName + LINK2_END),
                        new SpanInfo(LINK1, LINK1_END, captureeSpan),
                        new SpanInfo(LINK2, LINK2_END, capturerSpan));

        mActiveTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(tabProvider, true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        updateToolbarForTab(tab);
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        updateToolbarForTab(tab);
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        updateToolbarForTab(tab);
                    }

                    @Override
                    public void onUrlUpdated(Tab tab) {
                        updateToolbarForTab(tab);
                    }
                };

        mModel.set(TabSharingToolbarProperties.STOP_SHARING_CLICK_LISTENER, this::stopSharing);
        mModel.set(
                TabSharingToolbarProperties.SHARE_INSTEAD_CLICK_LISTENER,
                this::changeSourceToCurrentTab);

        updateToolbarForTab(mTabProvider.get());
    }

    /**
     * Updates the toolbar status text and the "Share this tab instead" button visibility based on
     * the currently active tab.
     *
     * @param tab The currently active {@link Tab}, or null if no tab is selected.
     */
    private void updateToolbarForTab(@Nullable Tab tab) {
        updateStatus(tab);
        updateShareInsteadButtonVisibility(tab);
    }

    private void updateStatus(@Nullable Tab currentTab) {
        if (currentTab == null) {
            return;
        }
        WebContents webContents = currentTab.getWebContents();

        if (webContents == mCapturee) {
            mModel.set(TabSharingToolbarProperties.STATUS_TEXT, mCaptureeStatus);
        } else if (webContents == mCapturer) {
            mModel.set(TabSharingToolbarProperties.STATUS_TEXT, mCapturerStatus);
        } else {
            mModel.set(TabSharingToolbarProperties.STATUS_TEXT, mOtherTabsStatus);
        }
    }

    private void updateShareInsteadButtonVisibility(@Nullable Tab currentTab) {
        mModel.set(
                TabSharingToolbarProperties.SHARE_INSTEAD_BUTTON_VISIBLE,
                shouldShowShareInsteadButton(currentTab));
    }

    private boolean shouldShowShareInsteadButton(@Nullable Tab currentTab) {
        if (!mBridge.isSourceSwitchingSupported() || currentTab == null) {
            return false;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null || webContents == mCapturee) {
            return false;
        }

        if (webContents == mCapturer) {
            // Prevent recursive "infinite mirror" video capture on the capturer tab (e.g. video
            // meeting) unless the capturing application explicitly requested self-capture via the
            // preferCurrentTab constraint.
            return mBridge.appPreferredCurrentTab();
        }

        return isTabPickable(currentTab);
    }

    private ClickableSpan buildClickToNavigateToTabSpan(WebContents webContents) {
        return new ClickableSpan() {
            @Override
            public void onClick(View view) {
                bringTabToFront(webContents);
            }

            @Override
            public void updateDrawState(TextPaint ds) {
                super.updateDrawState(ds);
                ds.setUnderlineText(false);
            }
        };
    }

    private void bringTabToFront(WebContents webContents) {
        Tab tab = TabUtils.fromWebContents(webContents);
        if (tab == null) {
            return;
        }

        MediaCaptureUtils.bringTabToFront(mContext, tab);
    }

    private void stopSharing() {
        mBridge.stopSharing();
    }

    private void changeSourceToCurrentTab() {
        if (MediaCaptureDevicesDispatcherAndroid.isSourceSwitchingInProgress(
                mBridge.getCapturer())) {
            return;
        }
        Tab currentTab = mTabProvider.get();
        if (currentTab != null && currentTab.getWebContents() != null) {
            mBridge.changeSource(currentTab.getWebContents());
        }
    }

    private boolean isTabPickable(Tab tab) {
        if (tab.isNativePage() || NativePage.isChromePageUrl(tab.getUrl(), tab.isIncognito())) {
            return false;
        }
        final WebContents webContents = tab.getWebContents();
        if (webContents == null) {
            return false;
        }

        return !MediaCaptureDevicesDispatcherAndroid.shouldFilterWebContents(
                mBridge.getCapturer(), webContents);
    }

    /** Cleans up resources. */
    public void destroy() {
        mActiveTabObserver.destroy();
    }
}
