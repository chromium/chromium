// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.ALL_KEYS;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.FAVICON;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.TITLE;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.URL;
import static org.chromium.chrome.browser.tab.TabLoadIfNeededCaller.ON_ACTIVITY_SHOWN_THEN_SHOW;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;

import android.app.PictureInPictureParams;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.SystemClock;
import android.util.Rational;

import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.PictureInPictureModeChangedInfo;
import androidx.core.util.Consumer;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentTransaction;
import androidx.lifecycle.Lifecycle.State;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/** Class that manages minimizing a Custom Tab into picture-in-picture. */
@RequiresApi(VERSION_CODES.O)
public class CustomTabMinimizationManager
        implements CustomTabMinimizeDelegate, Consumer<PictureInPictureModeChangedInfo> {
    // List of possible minimization events - maximize is effectively an 'un-PiP', whereas destroy
    // refers to the activity being finished either by user action or otherwise.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        MinimizationEvents.MINIMIZE,
        MinimizationEvents.MAXIMIZE,
        MinimizationEvents.DESTROY,
        MinimizationEvents.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface MinimizationEvents {
        int MINIMIZE = 0;
        int MAXIMIZE = 1;
        int DESTROY = 2;

        int COUNT = 3;
    }

    @VisibleForTesting static final Rational ASPECT_RATIO = new Rational(16, 9);
    private final AppCompatActivity mActivity;
    private final ActivityTabProvider mTabProvider;
    private final MinimizedCustomTabFeatureEngagementDelegate mFeatureEngagementDelegate;
    private final BrowserServicesIntentDataProvider mIntentData;
    private final Runnable mCloseTabRunnable;
    private long mMinimizationSystemTime;
    private boolean mMinimized;

    /**
     * @param activity The {@link AppCompatActivity} to minimize.
     * @param tabProvider The {@link ActivityTabProvider} that provides the Tab that will be
     *     minimized.
     * @param featureEngagementDelegate The {@link MinimizedCustomTabFeatureEngagementDelegate}.
     * @param closeTabRunnable The {@link Runnable} to close the Custom Tab when the minimized tab
     *     is dismissed.
     */
    public CustomTabMinimizationManager(
            AppCompatActivity activity,
            ActivityTabProvider tabProvider,
            MinimizedCustomTabFeatureEngagementDelegate featureEngagementDelegate,
            Runnable closeTabRunnable,
            BrowserServicesIntentDataProvider intentData) {
        mActivity = activity;
        mActivity.addOnPictureInPictureModeChangedListener(this);
        mTabProvider = tabProvider;
        mFeatureEngagementDelegate = featureEngagementDelegate;
        mCloseTabRunnable = closeTabRunnable;
        mIntentData = intentData;
    }

    public void destroy() {
        mActivity.removeOnPictureInPictureModeChangedListener(this);
    }

    /** Minimize the Custom Tab into picture-in-picture. */
    @Override
    public void minimize() {
        if (!mTabProvider.hasValue()) return;
        mFeatureEngagementDelegate.notifyUserEngaged();
        var builder = new PictureInPictureParams.Builder().setAspectRatio(ASPECT_RATIO);
        if (VERSION.SDK_INT >= VERSION_CODES.S) {
            builder.setSeamlessResizeEnabled(false);
        }
        mMinimized = mActivity.enterPictureInPictureMode(builder.build());
        mMinimizationSystemTime = SystemClock.elapsedRealtime();
    }

    @Override
    public void dismiss() {
        mCloseTabRunnable.run();
    }

    @Override
    public boolean isMinimized() {
        return mMinimized;
    }

    @Override
    public void accept(PictureInPictureModeChangedInfo pictureInPictureModeChangedInfo) {
        Tab tab = mTabProvider.get();
        assert tab != null;
        if (pictureInPictureModeChangedInfo.isInPictureInPictureMode()) {
            updateTabForMinimization(tab);
            CustomTabsConnection.getInstance().onMinimized(mIntentData.getSession());
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.MinimizedEvents",
                    MinimizationEvents.MINIMIZE,
                    MinimizationEvents.COUNT);
        } else {
            mMinimized = false;
            // We receive an update here when PiP is dismissed and the Activity is being stopped
            // before destruction. In that case, the state will be CREATED.
            var state = mActivity.getLifecycle().getCurrentState();
            if (state == State.CREATED || state == State.DESTROYED) {
                RecordHistogram.recordEnumeratedHistogram(
                        "CustomTabs.MinimizedEvents",
                        MinimizationEvents.DESTROY,
                        MinimizationEvents.COUNT);
                if (mMinimizationSystemTime != 0) {
                    RecordHistogram.recordTimesHistogram(
                            "CustomTabs.TimeElapsedSinceMinimized.Destroyed",
                            TimeUnit.MILLISECONDS.toSeconds(
                                    SystemClock.elapsedRealtime() - mMinimizationSystemTime));
                }
                mCloseTabRunnable.run();
                return;
            }

            updateTabForMaximization(tab);
            CustomTabsConnection.getInstance().onUnminimized(mIntentData.getSession());
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.MinimizedEvents",
                    MinimizationEvents.MAXIMIZE,
                    MinimizationEvents.COUNT);
            if (mMinimizationSystemTime != 0) {
                RecordHistogram.recordTimesHistogram(
                        "CustomTabs.TimeElapsedSinceMinimized.Maximized",
                        TimeUnit.MILLISECONDS.toSeconds(
                                SystemClock.elapsedRealtime() - mMinimizationSystemTime));
            }
        }
    }

    private void updateTabForMinimization(Tab tab) {
        if (tab == null) return;

        GURL url =
                DomDistillerUrlUtils.isDistilledPage(tab.getUrl())
                        ? tab.getOriginalUrl()
                        : tab.getUrl();

        PropertyModel model =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(TITLE, tab.getTitle())
                        .with(URL, url.getHost())
                        .with(FAVICON, TabFavicon.getBitmap(tab))
                        .build();
        var fragment = MinimizedCardDialogFragment.newInstance(model);
        FragmentTransaction transaction = mActivity.getSupportFragmentManager().beginTransaction();
        transaction.setTransition(FragmentTransaction.TRANSIT_NONE);
        transaction
                .add(android.R.id.content, fragment, MinimizedCardDialogFragment.TAG)
                .commitNow();

        tab.stopLoading();
        tab.hide(TabHidingType.ACTIVITY_HIDDEN);
        var webContents = tab.getWebContents();
        if (webContents != null) {
            webContents.suspendAllMediaPlayers();
            webContents.setAudioMuted(true);
        }
    }

    private void updateTabForMaximization(Tab tab) {
        if (tab == null) return;
        tab.show(FROM_USER, ON_ACTIVITY_SHOWN_THEN_SHOW);
        var webContents = tab.getWebContents();
        if (webContents != null) {
            webContents.setAudioMuted(false);
        }
        var fragment =
                (DialogFragment)
                        mActivity
                                .getSupportFragmentManager()
                                .findFragmentByTag(MinimizedCardDialogFragment.TAG);
        fragment.dismissNow();
    }
}
