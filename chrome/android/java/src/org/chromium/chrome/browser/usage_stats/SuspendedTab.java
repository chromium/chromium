// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.UserData;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.media.MediaCaptureDevicesDispatcherAndroid;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabViewProvider;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Represents the suspension page presented when a user tries to visit a site whose fully-qualified
 * domain name (FQDN) has been suspended via Digital Wellbeing.
 */
public class SuspendedTab extends EmptyTabObserver implements UserData, TabViewProvider {
    private static final String DIGITAL_WELLBEING_SITE_DETAILS_ACTION =
            "org.chromium.chrome.browser.usage_stats.action.SHOW_WEBSITE_DETAILS";
    private static final String EXTRA_FQDN_NAME =
            "org.chromium.chrome.browser.usage_stats.extra.FULLY_QUALIFIED_DOMAIN_NAME";
    private static final String TAG = "SuspendedTab";
    private static final Class<SuspendedTab> USER_DATA_KEY = SuspendedTab.class;

    public static boolean isShowing(Tab tab) {
        if (tab == null || !tab.isInitialized()) return false;
        SuspendedTab suspendedTab = get(tab);
        return suspendedTab != null && suspendedTab.isShowing();
    }

    /**
     * @return The SuspendedTab instance for the given Tab object. This can never return null, but
     *         is not safe to call if the tab has been destroyed.
     */
    public static SuspendedTab from(
            Tab tab, Supplier<TabContentManager> tabContentManagerSupplier) {
        assert tab.isInitialized();
        SuspendedTab suspendedTab = get(tab);
        if (suspendedTab == null) {
            suspendedTab =
                    tab.getUserDataHost()
                            .setUserData(
                                    USER_DATA_KEY,
                                    new SuspendedTab(tab, tabContentManagerSupplier));
        }
        return suspendedTab;
    }

    public static SuspendedTab get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    @Override
    public @ColorInt int getBackgroundColor(Context context) {
        return SemanticColorUtils.getDefaultBgColor(context);
    }

    private final Tab mTab;
    private final Supplier<TabContentManager> mTabContentManagerSupplier;
    private View mView;
    private String mFqdn;

    private SuspendedTab(Tab tab, Supplier<TabContentManager> tabContentManagerSupplier) {
        mTab = tab;
        mTabContentManagerSupplier = tabContentManagerSupplier;
    }

    /**
     * Show the suspended tab UI within the root view of the associated tab. This will stop loading
     * of mTab so that the page is not also rendered. If the suspended tab is already showing, this
     * will update its fqdn to the given one.
     */
    public void show(String fqdn) {
        mFqdn = fqdn;
        mTab.addObserver(this);
        mTab.stopLoading();

        WebContents webContents = mTab.getWebContents();
        if (webContents != null) {
            webContents.updateWebContentsVisibility(Visibility.HIDDEN);
            webContents.suspendAllMediaPlayers();
            webContents.setAudioMuted(true);
            if (MediaCaptureDevicesDispatcherAndroid.isCapturingAudio(webContents)
                    || MediaCaptureDevicesDispatcherAndroid.isCapturingVideo(webContents)
                    || MediaCaptureDevicesDispatcherAndroid.isCapturingScreen(webContents)) {
                MediaCaptureDevicesDispatcherAndroid.notifyStopped(webContents);
            }
        }

        InfoBarContainer infoBarContainer = InfoBarContainer.get(mTab);
        if (infoBarContainer != null) {
            infoBarContainer.setHidden(true);
        }

        if (isViewAttached()) {
            updateFqdnText();
        } else {
            attachView();
        }

        TabContentManager tabContentManager = mTabContentManagerSupplier.get();
        if (tabContentManager != null) {
            // We have to wait for the view to layout to cache a new thumbnail for it; otherwise,
            // its width and height won't be available yet.
            mView.post(
                    () -> {
                        tabContentManager.removeTabThumbnail(mTab.getId());
                        tabContentManager.cacheTabThumbnail(mTab);
                    });
        }
    }

    /** Remove the suspended tab UI if it's currently being shown. */
    public void removeIfPresent() {
        removeViewIfPresent();

        WebContents webContents = mTab.getWebContents();
        if (webContents != null) {
            webContents.updateWebContentsVisibility(Visibility.VISIBLE);
            webContents.setAudioMuted(false);
        }

        mView = null;
        mFqdn = null;
    }

    /** @return the fqdn this SuspendedTab is currently showing for; null if not showing. */
    public String getFqdn() {
        return mFqdn;
    }

    /** @return Whether this SuspendedTab is currently showing. */
    public boolean isShowing() {
        return mFqdn != null;
    }

    @VisibleForTesting
    boolean isViewAttached() {
        return mView != null && mTab.getTabViewManager().isShowing(this);
    }

    private View createView() {
        Context context = mTab.getContext();
        LayoutInflater inflater = LayoutInflater.from(context);

        View suspendedTabView = inflater.inflate(R.layout.suspended_tab, null);
        suspendedTabView.setLayoutParams(
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        return suspendedTabView;
    }

    private void attachView() {
        assert mView == null;

        mView = createView();
        mTab.getTabViewManager().addTabViewProvider(this);
        updateFqdnText();
    }

    private void updateFqdnText() {
        Context context = mTab.getContext();
        TextView explanationText = mView.findViewById(R.id.suspended_tab_explanation);
        explanationText.setText(
                context.getString(R.string.usage_stats_site_paused_explanation, mFqdn));
        setSettingsLinkClickListener();
    }

    private void setSettingsLinkClickListener() {
        Context context = mTab.getContext();
        View settingsLink = mView.findViewById(R.id.suspended_tab_settings_button);
        settingsLink.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        Intent intent = new Intent(DIGITAL_WELLBEING_SITE_DETAILS_ACTION);
                        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        intent.putExtra(EXTRA_FQDN_NAME, mFqdn);
                        intent.putExtra(
                                Intent.EXTRA_PACKAGE_NAME,
                                ContextUtils.getApplicationContext().getPackageName());
                        try {
                            context.startActivity(intent);
                        } catch (ActivityNotFoundException e) {
                            Log.e(TAG, "No activity found for site details intent", e);
                        }
                    }
                });
    }

    private void removeViewIfPresent() {
        mTab.getTabViewManager().removeTabViewProvider(this);
        mView = null;
    }

    // TabObserver implementation.
    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        if (window == null) {
            removeViewIfPresent();
        } else {
            attachView();
        }
    }

    // UserData implementation.
    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }

    @Override
    public int getTabViewProviderType() {
        return Type.SUSPENDED_TAB;
    }

    @Override
    public View getView() {
        return mView;
    }
}
