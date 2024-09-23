// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter.reportJavaException;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.ALL_KEYS;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.FAVICON;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.TITLE;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.URL;
import static org.chromium.chrome.browser.tab.TabLoadIfNeededCaller.ON_ACTIVITY_SHOWN_THEN_SHOW;
import static org.chromium.chrome.browser.tab.TabSelectionType.FROM_USER;

import android.app.PictureInPictureParams;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Rational;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.PictureInPictureModeChangedInfo;
import androidx.core.util.Consumer;
import androidx.lifecycle.Lifecycle.State;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.SaveInstanceStateObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.concurrent.TimeUnit;

/** Class that manages minimizing a Custom Tab into picture-in-picture. */
public class CustomTabMinimizationManager
        implements CustomTabMinimizeDelegate,
                Consumer<PictureInPictureModeChangedInfo>,
                SaveInstanceStateObserver {
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

    @VisibleForTesting static WeakReference<CustomTabMinimizeDelegate> sLastMinimizeDelegate;

    @VisibleForTesting static final String KEY_IS_CCT_MINIMIZED = "isCctMinimized";

    @VisibleForTesting
    static final String KEY_CCT_MINIMIZATION_SYSTEM_TIME = "cctMinimizationSystemTime";

    // --- For debugging
    private static final String TAG = "CTMinimizationMgr";
    private static final String TASK_DISPLAY_AREA_NPE_STR =
            "com.android.server.wm.TaskDisplayArea.positionStackAtTop";
    private static final String DEVICE_DOES_NOT_SUPPORT_ISE_STR =
            "Device doesn't support picture-in-picture mode";
    private static final String ACTIVITY_DOES_NOT_SUPPORT_ISE_STR =
            "Current activity does not support picture-in-picture";
    private static final String ROOT_TASK_IAE_STR = "addRootTaskReferenceIfNeeded: root pinned";
    // ---

    private final AppCompatActivity mActivity;
    private final ActivityTabProvider mTabProvider;
    private final MinimizedCustomTabFeatureEngagementDelegate mFeatureEngagementDelegate;
    private final BrowserServicesIntentDataProvider mIntentData;
    private final Runnable mCloseTabRunnable;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final Supplier<Bundle> mSavedInstanceStateSupplier;
    private MinimizedCardCoordinator mCoordinator;
    private PropertyModel mModel;
    private long mMinimizationSystemTime;
    private boolean mMinimized;

    /**
     * @param activity The {@link AppCompatActivity} to minimize.
     * @param tabProvider The {@link ActivityTabProvider} that provides the Tab that will be
     *     minimized.
     * @param featureEngagementDelegate The {@link MinimizedCustomTabFeatureEngagementDelegate}.
     * @param closeTabRunnable The {@link Runnable} to close the Custom Tab when the minimized tab
     *     is dismissed.
     * @param intentData The {@link BrowserServicesIntentDataProvider}.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher}.
     * @param savedInstanceStateSupplier {@link Supplier} for the saved instance state.
     */
    public CustomTabMinimizationManager(
            AppCompatActivity activity,
            ActivityTabProvider tabProvider,
            MinimizedCustomTabFeatureEngagementDelegate featureEngagementDelegate,
            Runnable closeTabRunnable,
            BrowserServicesIntentDataProvider intentData,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Supplier<Bundle> savedInstanceStateSupplier) {
        mActivity = activity;
        mTabProvider = tabProvider;
        mFeatureEngagementDelegate = featureEngagementDelegate;
        mCloseTabRunnable = closeTabRunnable;
        mIntentData = intentData;
        mLifecycleDispatcher = lifecycleDispatcher;
        mSavedInstanceStateSupplier = savedInstanceStateSupplier;

        mLifecycleDispatcher.register(this);

        maybeInitializeAsMinimized();
    }

    public void destroy() {
        mActivity.removeOnPictureInPictureModeChangedListener(this);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        if (mMinimized) {
            outState.putBoolean(KEY_IS_CCT_MINIMIZED, true);
            outState.putLong(KEY_CCT_MINIMIZATION_SYSTEM_TIME, mMinimizationSystemTime);
            putIntoBundleFromModel(outState, mModel);
        }
    }

    /** Minimize the Custom Tab into picture-in-picture. */
    @Override
    public void minimize() {
        if (mMinimized) return;
        if (!mTabProvider.hasValue()) return;
        mFeatureEngagementDelegate.notifyUserEngaged();
        var builder = new PictureInPictureParams.Builder().setAspectRatio(ASPECT_RATIO);
        if (VERSION.SDK_INT >= VERSION_CODES.S) {
            builder.setSeamlessResizeEnabled(false);
        }

        maybeDismissLastMinimizedTab();

        // Sometimes an exception may be thrown by the framework code. If it's something we've seen
        // before, catch it and report with extra debug information without crashing. Otherwise,
        // rethrow so we don't inadvertently hide other crashes.
        try {
            mMinimized = mActivity.enterPictureInPictureMode(builder.build());
            recordMinimizeSuccess(/* success= */ true);
        } catch (NullPointerException e) {
            if (doesExceptionMatch(e, TASK_DISPLAY_AREA_NPE_STR)) {
                String msg = "NullPointerException";
                reportException(TASK_DISPLAY_AREA_NPE_STR, msg, e);
            } else {
                throw e;
            }
        } catch (IllegalStateException e) {
            if (doesExceptionMatch(e, DEVICE_DOES_NOT_SUPPORT_ISE_STR)) {
                String msg = "Device doesn't support picture-in-picture mode.";
                reportException(DEVICE_DOES_NOT_SUPPORT_ISE_STR, msg, e);
            } else if (doesExceptionMatch(e, ACTIVITY_DOES_NOT_SUPPORT_ISE_STR)) {
                String msg =
                        "Current activity does not support picture-in-picture. Activity class: "
                                + mActivity.getLocalClassName();
                reportException(ACTIVITY_DOES_NOT_SUPPORT_ISE_STR, msg, e);
            } else {
                throw e;
            }
        } catch (IllegalArgumentException e) {
            if (doesExceptionMatch(e, ROOT_TASK_IAE_STR)) {
                String msg = "IllegalArgumentException";
                reportException(ROOT_TASK_IAE_STR, msg, e);
            } else {
                throw e;
            }
        }
        if (!mMinimized) {
            recordMinimizeSuccess(/* success= */ false);
            Toast.makeText(mActivity, R.string.minimize_failure_toast, Toast.LENGTH_SHORT).show();
            return;
        }

        maybeSaveLastMinimizeDelegate();

        mActivity.addOnPictureInPictureModeChangedListener(this);
        notifyObservers(true);
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
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void accept(PictureInPictureModeChangedInfo pictureInPictureModeChangedInfo) {
        if (!mMinimized) return;

        Tab tab = mTabProvider.get();

        if (tab == null) {
            boolean wasInitializedMinimized =
                    mSavedInstanceStateSupplier.hasValue()
                            && mSavedInstanceStateSupplier.get().getBoolean(KEY_IS_CCT_MINIMIZED);
            String msg =
                    "Tab is null. Activity state is "
                            + mActivity.getLifecycle().getCurrentState()
                            + ". wasInitializedMinimized: "
                            + wasInitializedMinimized
                            + ". isInPip: "
                            + pictureInPictureModeChangedInfo.isInPictureInPictureMode();
            Log.e(TAG, msg);
            reportJavaException(new Exception(msg));
        }

        if (pictureInPictureModeChangedInfo.isInPictureInPictureMode()) {
            showMinimizedCard(/* fromSavedState= */ false);
            updateTabForMinimization(tab);
            CustomTabsConnection.getInstance().onMinimized(mIntentData.getSession());
            RecordHistogram.recordEnumeratedHistogram(
                    "CustomTabs.MinimizedEvents",
                    MinimizationEvents.MINIMIZE,
                    MinimizationEvents.COUNT);
        } else {
            mActivity.removeOnPictureInPictureModeChangedListener(this);
            notifyObservers(false);
            maybeClearLastMinimizedTabRef();
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

            mMinimized = false;
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

    private void maybeInitializeAsMinimized() {
        mMinimized =
                mSavedInstanceStateSupplier.hasValue()
                        && mSavedInstanceStateSupplier.get().getBoolean(KEY_IS_CCT_MINIMIZED);

        if (mMinimized) {
            mLifecycleDispatcher.register(
                    new InflationObserver() {
                        @Override
                        public void onPreInflationStartup() {}

                        @Override
                        public void onPostInflationStartup() {
                            maybeSaveLastMinimizeDelegate();
                            mActivity.addOnPictureInPictureModeChangedListener(
                                    CustomTabMinimizationManager.this);
                            showMinimizedCard(/* fromSavedState= */ true);
                            notifyObservers(true);
                            mMinimizationSystemTime =
                                    mSavedInstanceStateSupplier
                                            .get()
                                            .getLong(KEY_CCT_MINIMIZATION_SYSTEM_TIME);
                            mLifecycleDispatcher.unregister(this);
                        }
                    });
        }
    }

    private void showMinimizedCard(boolean fromSavedState) {
        if (fromSavedState) {
            assert mSavedInstanceStateSupplier.hasValue();
            mModel = toModel(mSavedInstanceStateSupplier.get());
        } else {
            Tab tab = mTabProvider.get();
            if (tab == null) return;
            GURL url =
                    DomDistillerUrlUtils.isDistilledPage(tab.getUrl())
                            ? tab.getOriginalUrl()
                            : tab.getUrl();
            String host =
                    UrlFormatter.formatUrlForSecurityDisplay(url, SchemeDisplay.OMIT_CRYPTOGRAPHIC);
            String title =
                    ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL.equals(host) ? "" : tab.getTitle();
            mModel =
                    new PropertyModel.Builder(ALL_KEYS)
                            .with(TITLE, title)
                            .with(URL, host)
                            .with(FAVICON, TabFavicon.getBitmap(tab))
                            .build();
        }
        mCoordinator =
                new MinimizedCardCoordinator(
                        mActivity, mActivity.findViewById(android.R.id.content), mModel);
    }

    private void updateTabForMinimization(Tab tab) {
        if (tab == null) return;

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
        if (mCoordinator != null) {
            mCoordinator.dismiss();
        }
    }

    private void notifyObservers(boolean minimized) {
        for (var obs : mObservers) {
            obs.onMinimizationChanged(minimized);
        }
    }

    private CustomTabMinimizeDelegate getLastMinimizeDelegate() {
        if (sLastMinimizeDelegate == null) return null;

        return sLastMinimizeDelegate.get();
    }

    private void maybeSaveLastMinimizeDelegate() {
        if (VERSION.SDK_INT < VERSION_CODES.R || VERSION.SDK_INT >= VERSION_CODES.TIRAMISU) return;

        sLastMinimizeDelegate = new WeakReference<>(this);
    }

    private void clearLastMinimizeDelegate() {
        if (sLastMinimizeDelegate == null) return;

        sLastMinimizeDelegate.clear();
        sLastMinimizeDelegate = null;
    }

    private void maybeDismissLastMinimizedTab() {
        // On Android R and S, minimizing a tab while there is already an active PiP window unPiPs
        // the current one instead of closing the Activity. This can cause some weird issues if it
        // happens multiple times back-to-back. To prevent these issues, we dismiss the last
        // minimized Custom Tab. This only works for the Minimized Custom Tabs feature and doesn't
        // affect other uses of PiP such as fullscreen video.
        if (VERSION.SDK_INT < VERSION_CODES.R || VERSION.SDK_INT >= VERSION_CODES.TIRAMISU) return;

        var lastMinimized = getLastMinimizeDelegate();
        if (lastMinimized != null) {
            lastMinimized.dismiss();
            clearLastMinimizeDelegate();
        }
    }

    private void maybeClearLastMinimizedTabRef() {
        var lastMinimized = getLastMinimizeDelegate();
        if (lastMinimized == this) {
            clearLastMinimizeDelegate();
        }
    }

    private static void putIntoBundleFromModel(Bundle out, PropertyModel model) {
        if (model == null) return;

        out.putString(TITLE.toString(), model.get(TITLE));
        out.putString(URL.toString(), model.get(URL));
        out.putParcelable(FAVICON.toString(), model.get(FAVICON));
    }

    private static PropertyModel toModel(Bundle bundle) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(TITLE, bundle.getString(TITLE.toString()))
                .with(URL, bundle.getString(URL.toString()))
                .with(FAVICON, bundle.getParcelable(FAVICON.toString()))
                .build();
    }

    private boolean doesExceptionMatch(Exception e, String subString) {
        return e.getMessage() != null && e.getMessage().contains(subString);
    }

    private void reportException(String key, String msg, Exception e) {
        String msgWithState =
                msg + " -- ActivityState: " + mLifecycleDispatcher.getCurrentActivityState();
        Log.e(TAG, msgWithState, e);
        reportJavaException(new Exception(msg, e));
    }

    private void recordMinimizeSuccess(boolean success) {
        RecordHistogram.recordBooleanHistogram("CustomTabs.Minimized.MinimizeSuccess", success);
    }
}
