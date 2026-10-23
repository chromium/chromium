// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static android.view.Display.INVALID_DISPLAY;

import android.app.Activity;
import android.app.ActivityManager.AppTask;
import android.app.ActivityOptions;
import android.app.InfeasibleActivityOptionsException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.jni_zero.CalledByNativeForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.IncognitoCctCallerId;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabDelegateFactory;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.PopupCreator;
import org.chromium.chrome.browser.customtabs.features.desktop_popup_header.DesktopPopupHeaderUtils;
import org.chromium.chrome.browser.media.AutoPictureInPictureTabHelper;
import org.chromium.chrome.browser.media.DocumentPictureInPictureActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.WindowResizePrecheckResult;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Handles launching new popup windows as CCTs and Document Picture-in-Picture windows. */
@NullMarked
public class PopupCreatorImpl implements PopupCreator {
    public static final String EXTRA_REQUESTED_WINDOW_FEATURES =
            "chrome.browser.app.tab_activity_glue.PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES";

    private static final String TAG = "PopupCreatorImpl";

    /** What a popup is created from, i.e. which {@link PopupCreator} method creates it. */
    @IntDef({
        PopupLaunchSource.NEW_POPUP,
        PopupLaunchSource.FROM_WEB_CONTENTS,
        PopupLaunchSource.FROM_TAB
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface PopupLaunchSource {
        int NEW_POPUP = 0;
        int FROM_WEB_CONTENTS = 1;
        int FROM_TAB = 2;
    }

    /** How a popup is launched, decided before the launch. */
    @IntDef({
        PopupLaunchMode.MOVABLE_TASK_REQUIRED,
        PopupLaunchMode.MOVABLE_TASK_NOT_REQUIRED,
        PopupLaunchMode.FALLBACK_TO_TAB
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface PopupLaunchMode {
        int MOVABLE_TASK_REQUIRED = 0;
        int MOVABLE_TASK_NOT_REQUIRED = 1;
        int FALLBACK_TO_TAB = 2;
    }

    // LINT.IfChange(PopupLaunchOutcome)
    @IntDef({
        PopupLaunchOutcome.MOVABLE_TASK_LAUNCHED,
        PopupLaunchOutcome.NON_MOVABLE_TASK_LAUNCHED_NO_PERMISSION,
        PopupLaunchOutcome.NON_MOVABLE_TASK_LAUNCHED_BELOW_API37,
        PopupLaunchOutcome.NON_MOVABLE_TASK_LAUNCHED_AFTER_INFEASIBLE,
        PopupLaunchOutcome.FALLBACK_TO_TAB,
        PopupLaunchOutcome.FAILED_INFEASIBLE_OPTIONS,
        PopupLaunchOutcome.FAILED_SECURITY,
        PopupLaunchOutcome.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface PopupLaunchOutcome {
        int MOVABLE_TASK_LAUNCHED = 0;
        int NON_MOVABLE_TASK_LAUNCHED_NO_PERMISSION = 1;
        int NON_MOVABLE_TASK_LAUNCHED_BELOW_API37 = 2;
        int NON_MOVABLE_TASK_LAUNCHED_AFTER_INFEASIBLE = 3;
        int FALLBACK_TO_TAB = 4;
        int FAILED_INFEASIBLE_OPTIONS = 5;
        int FAILED_SECURITY = 6;
        int COUNT = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/enums.xml:PopupLaunchOutcome)

    /** The popup launch in progress, read by {@link #tryStartActivity}. */
    private static final class PendingPopupLaunch {
        final @PopupLaunchSource int mLaunchSource;
        final @PopupLaunchMode int mLaunchMode;

        /** Options to retry with after InfeasibleActivityOptionsException; null if no retry. */
        final @Nullable Bundle mRetryOptions;

        PendingPopupLaunch(
                @PopupLaunchSource int launchSource,
                @PopupLaunchMode int launchMode,
                @Nullable Bundle retryOptions) {
            mLaunchSource = launchSource;
            mLaunchMode = launchMode;
            mRetryOptions = retryOptions;
        }
    }

    /** Starts the Activity of a popup for {@link #launchPopup}. */
    private interface ActivityStarter {
        /**
         * Starts the Activity, synchronously calling {@link #tryStartActivity}.
         *
         * @param activityOptions The {@link ActivityOptions} bundle to start the Activity with.
         * @return Whether the Activity was started.
         */
        boolean start(Bundle activityOptions);
    }

    private static @Nullable ReparentingTask sReparentingTaskForTesting;
    private static @Nullable Boolean sPredictPopupWindowBoundsForTesting;
    private static @Nullable Boolean sMoveTabToNewPopupResultForTesting;
    private static @Nullable Boolean sMoveToNewDocumentPiPWindowResultForTesting;
    private static @Nullable Boolean sSetMovableTaskRequiredForPopupsForTesting;
    private static @Nullable Boolean sTryStartActivityResultForTesting;

    private @Nullable PendingPopupLaunch mPendingPopupLaunch;

    @Override
    public boolean createNewPopup(
            Context context,
            boolean isIncognito,
            @Nullable WindowFeatures windowFeatures,
            @Nullable Bundle additionalIntentExtras) {
        final Intent intent =
                createTrustedPopupIntent(windowFeatures, isIncognito, additionalIntentExtras);
        return launchPopup(
                PopupLaunchSource.NEW_POPUP,
                getPopupLaunchMode(),
                resolveTargetDisplayAndBounds(windowFeatures),
                /* sourceWindow= */ null,
                options -> tryStartActivity(context, intent, options));
    }

    @Override
    public boolean createNewPopupFromWebContents(
            Context context,
            Profile profile,
            WebContents webContents,
            @Nullable WindowFeatures windowFeatures,
            @Nullable Bundle additionalIntentExtras) {
        // Only allow popups for the main incognito profile. Other OTR profiles
        // cannot have popups. The caller transfers ownership of webContents to
        // this method, so if popup creation fails we must destroy webContents.
        if (profile.isOffTheRecord() && !profile.isPrimaryOtrProfile()) {
            webContents.destroy();
            return false;
        }

        TabDelegateFactory delegateFactory = CustomTabDelegateFactory.createEmpty();
        Tab tab = TabBuilder.createDetachedSpareTab(context, delegateFactory, profile, webContents);

        final Intent intent =
                createTrustedPopupIntent(
                        windowFeatures, tab.isIncognitoBranded(), additionalIntentExtras);
        intent.putExtra(IntentHandler.EXTRA_SKIP_LOAD_ON_REPARENTING, true);

        boolean success =
                launchPopup(
                        PopupLaunchSource.FROM_WEB_CONTENTS,
                        getPopupLaunchMode(),
                        resolveTargetDisplayAndBounds(windowFeatures),
                        tab.getWindowAndroid(),
                        options ->
                                getReparentingTask(tab)
                                        .begin(tab.getContext(), intent, options, null));
        if (!success) {
            tab.destroy();
        }
        return success;
    }

    @Override
    public boolean moveTabToNewPopup(Tab tab, WindowFeatures windowFeatures) {
        if (sMoveTabToNewPopupResultForTesting != null) {
            return sMoveTabToNewPopupResultForTesting;
        }

        Profile profile = tab.getProfile();
        // Only allow popups for the main incognito profile. Other OTR profiles
        // cannot have popups.
        if (profile != null && profile.isOffTheRecord() && !profile.isPrimaryOtrProfile()) {
            return false;
        }

        Pair<DisplayAndroid, Rect> targetDisplayAndBounds =
                resolveTargetDisplayAndBounds(windowFeatures);
        @PopupLaunchMode
        int launchMode =
                getPopupLaunchMode(
                        tab, targetDisplayAndBounds != null ? targetDisplayAndBounds.first : null);
        if (launchMode == PopupLaunchMode.FALLBACK_TO_TAB) {
            recordOutcome(PopupLaunchSource.FROM_TAB, PopupLaunchOutcome.FALLBACK_TO_TAB);
            return false;
        }

        final Intent intent =
                createTrustedPopupIntent(
                        windowFeatures,
                        tab.isIncognitoBranded(),
                        /* additionalIntentExtras= */ null);
        intent.putExtra(IntentHandler.EXTRA_SKIP_LOAD_ON_REPARENTING, true);

        return launchPopup(
                PopupLaunchSource.FROM_TAB,
                launchMode,
                targetDisplayAndBounds,
                tab.getWindowAndroid(),
                options -> getReparentingTask(tab).begin(tab.getContext(), intent, options, null));
    }

    @Override
    public boolean moveWebContentsToNewDocumentPictureInPictureWindow(
            @Nullable Activity srcActivity,
            WebContents webContents,
            PictureInPictureWindowOptions windowOptions) {
        if (sMoveToNewDocumentPiPWindowResultForTesting != null) {
            return sMoveToNewDocumentPiPWindowResultForTesting;
        }

        WebContents opener = webContents.getDocumentPictureInPictureOpener();
        AutoPictureInPictureTabHelper helper =
                opener != null ? AutoPictureInPictureTabHelper.fromWebContents(opener) : null;
        Rect cachedBounds =
                helper != null ? helper.getCachedWindowBounds(windowOptions.windowBounds) : null;

        if (cachedBounds != null) {
            windowOptions =
                    new PictureInPictureWindowOptions(
                            cachedBounds, windowOptions.disallowReturnToOpener);
        }

        final ActivityOptions activityOptions =
                createDocumentPipActivityOptions(windowOptions.windowBounds);
        if (activityOptions == null) {
            return false;
        }

        final Intent intent = initializeDocumentPipIntent(webContents, windowOptions);
        return tryStartActivity(
                srcActivity == null ? ContextUtils.getApplicationContext() : srcActivity,
                intent,
                activityOptions.toBundle());
    }

    /**
     * Adjusts window bounds of given {@link ChromeActivity} to match requested {@link
     * WindowFeatures} using at most one {@link android.app.ActivityManager.AppTask#moveTaskTo} call
     * on best-effort basis.
     */
    public static void adjustWindowBoundsToRequested(
            ChromeActivity activity, @Nullable WindowFeatures requestedWindowFeaturesDp) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.CINNAMON_BUN) {
            return;
        }

        final ChromeAndroidTask task =
                ChromeAndroidTaskTrackerFactory.getInstance().get(activity.getTaskId());
        if (task == null || task.canResize() != WindowResizePrecheckResult.OK) {
            Log.v(TAG, "adjustWindowBounds: the task cannot be resized -- bailing out");
            return;
        }

        if (requestedWindowFeaturesDp == null) {
            Log.v(TAG, "adjustWindowBounds: requestedWindowFeaturesDp is null -- bailing out");
            return;
        }

        final Tab popupTab = activity.getActivityTabProvider().get();
        if (popupTab == null) {
            Log.w(TAG, "adjustWindowBounds: popupTab is null -- bailing out");
            return;
        }
        final WebContents webContents = popupTab.getWebContents();
        if (webContents == null) {
            Log.w(TAG, "adjustWindowBounds: webContents is null -- bailing out");
            return;
        }

        final int realViewportWidthDp = webContents.getWidth();
        final int realViewportHeightDp = webContents.getHeight();
        final Rect realWindowBoundsPx =
                new Rect(activity.getWindowManager().getCurrentWindowMetrics().getBounds());
        final Rect targetWindowBoundsPx = new Rect(realWindowBoundsPx);
        Log.v(
                TAG,
                "adjustWindowBounds: current viewport size = (%d x %d) [dp]",
                realViewportWidthDp,
                realViewportHeightDp);
        Log.v(TAG, "adjustWindowBounds: top-level window bounds = %s [px]", realWindowBoundsPx);
        Log.v(
                TAG,
                "adjustWindowBounds: requested WindowFeatures = %s [dp]",
                requestedWindowFeaturesDp);

        if (requestedWindowFeaturesDp.width != null && requestedWindowFeaturesDp.height != null) {
            final WindowAndroid windowAndroid = activity.getWindowAndroid();
            if (windowAndroid == null) {
                Log.w(
                        TAG,
                        "adjustWindowBounds: activity.getWindowAndroid() is null -- bailing out");
                return;
            }

            DisplayAndroid display = windowAndroid.getDisplay();

            final int widthDiffDp = requestedWindowFeaturesDp.width - realViewportWidthDp;
            final int heightDiffDp = requestedWindowFeaturesDp.height - realViewportHeightDp;

            final int widthDiffPx = DisplayUtil.dpToPx(display, widthDiffDp);
            final int heightDiffPx = DisplayUtil.dpToPx(display, heightDiffDp);

            targetWindowBoundsPx.right += widthDiffPx;
            targetWindowBoundsPx.bottom += heightDiffPx;

            final boolean isPopupOpenedCrossDisplay =
                    getDisplayIdFromTabId(popupTab.getParentId()) != display.getDisplayId();
            final String histogramVariant =
                    isPopupOpenedCrossDisplay ? "CrossDisplay" : "InDisplay";

            // If the display's dipScale is less than 1 it may happen that the difference in dps is
            // non-zero while the same difference in px is zero. In such case we consider it a
            // success as we could not have done anything better than being pixel perfect.

            RecordHistogram.recordBooleanHistogram(
                    "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaWidth.Outcome."
                            + histogramVariant,
                    widthDiffPx != 0);
            if (widthDiffPx != 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaWidth.Positive."
                                + histogramVariant,
                        Math.abs(widthDiffDp));
            }

            RecordHistogram.recordBooleanHistogram(
                    "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaHeight.Outcome."
                            + histogramVariant,
                    heightDiffPx != 0);
            if (heightDiffPx != 0) {
                RecordHistogram.recordCount1000Histogram(
                        "Android.MultiWindowMode.PopupBoundsAdjustment.DeltaHeight.Positive."
                                + histogramVariant,
                        Math.abs(heightDiffDp));
            }

            Log.v(TAG, "adjustWindowBounds: widthDiffDp = %d dp", widthDiffDp);
            Log.v(TAG, "adjustWindowBounds: widthDiffPx = %d px", widthDiffPx);
            Log.v(TAG, "adjustWindowBounds: heightDiffDp = %d dp", heightDiffDp);
            Log.v(TAG, "adjustWindowBounds: heightDiffPx = %d px", heightDiffPx);
            Log.v(TAG, "adjustWindowBounds: display.getDipScale() = %s", display.getDipScale());

            final InsetObserver insetObserver = windowAndroid.getInsetObserver();
            Insets windowInsetsOnSourceDisplay = Insets.NONE;
            if (insetObserver != null && insetObserver.getLastRawWindowInsets() != null) {
                windowInsetsOnSourceDisplay =
                        insetObserver
                                .getLastRawWindowInsets()
                                .getInsets(WindowInsetsCompat.Type.captionBar());
            }
            Log.v(
                    TAG,
                    "adjustWindowBounds: current window insets = %s [px]",
                    windowInsetsOnSourceDisplay);
            Log.v(
                    TAG,
                    "adjustWindowBounds: getTopControlsHeight from activity's BCM = %d px",
                    activity.getBrowserControlsManager().getTopControlsHeight());
            Log.v(
                    TAG,
                    "adjustWindowBounds: getTopControlsHairlineHeight from activity's BCM = %d px",
                    activity.getBrowserControlsManager().getTopControlsHairlineHeight());
        }

        if (realWindowBoundsPx.equals(targetWindowBoundsPx)) {
            Log.v(TAG, "adjustWindowBounds: perfect match -- no repositioning required");
            return;
        }
        Log.v(
                TAG,
                "adjustWindowBounds: repositioning required -- current bounds = %s, target bounds ="
                        + " %s",
                realWindowBoundsPx,
                targetWindowBoundsPx);

        final AppTask appTask = AndroidTaskUtils.getAppTaskFromId(activity, activity.getTaskId());
        if (appTask == null) {
            Log.w(TAG, "adjustWindowBounds: cannot find the AppTask -- bailing out");
            return;
        }

        Log.v(TAG, "adjustWindowBounds: dispatching the moveTaskTo call");
        AndroidTaskUtils.moveTaskTo(
                appTask, INVALID_DISPLAY /* current display */, targetWindowBoundsPx);
    }

    // TODO(crbug.com/576054107): Move tryStartActivity out of PopupCreatorImpl and share it with
    // tab reparenting, or give tab reparenting its own start-Activity logic.
    @Override
    public boolean tryStartActivity(
            Context context, Intent intent, @Nullable Bundle activityOptions) {
        if (sTryStartActivityResultForTesting != null) {
            return sTryStartActivityResultForTesting;
        }
        final PendingPopupLaunch pendingPopupLaunch = mPendingPopupLaunch;
        final @PopupLaunchSource @Nullable Integer launchSource =
                pendingPopupLaunch != null ? pendingPopupLaunch.mLaunchSource : null;
        try {
            context.startActivity(intent, activityOptions);
            recordOutcome(
                    launchSource,
                    pendingPopupLaunch != null
                                    && pendingPopupLaunch.mLaunchMode
                                            == PopupLaunchMode.MOVABLE_TASK_REQUIRED
                            ? PopupLaunchOutcome.MOVABLE_TASK_LAUNCHED
                            : nonMovableTaskLaunchOutcome());
            return true;
        } catch (SecurityException e) {
            Log.w(TAG, "tryStartActivity: no permission to start the activity", e);
            recordOutcome(launchSource, PopupLaunchOutcome.FAILED_SECURITY);
            return false;
        } catch (Throwable e) {
            if (!(Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN
                    && e instanceof InfeasibleActivityOptionsException)) {
                throw e;
            }
            Log.w(TAG, "tryStartActivity: startActivity threw InfeasibleActivityOptionsException");
            if (pendingPopupLaunch == null || pendingPopupLaunch.mRetryOptions == null) {
                recordOutcome(launchSource, PopupLaunchOutcome.FAILED_INFEASIBLE_OPTIONS);
                return false;
            }
            try {
                context.startActivity(intent, pendingPopupLaunch.mRetryOptions);
                recordOutcome(
                        launchSource,
                        PopupLaunchOutcome.NON_MOVABLE_TASK_LAUNCHED_AFTER_INFEASIBLE);
                return true;
            } catch (SecurityException retryFailure) {
                recordOutcome(launchSource, PopupLaunchOutcome.FAILED_SECURITY);
                return false;
            }
        }
    }

    /**
     * Predicts the bounds of the window that will host a popup with the requested viewport bounds.
     * The window keeps the top-left corner of the viewport and grows to the right and bottom:
     *
     * <pre>
     *   |<------ requested viewport width ------>|<- deltaWidthPx ->|
     *   +-----------------------------------------------------------+  ---
     *   | Caption bar (+ popup header overflow below it)            |   ^
     *   +-----------------------------------------------------------+   | deltaHeightPx
     *   | Custom Tab toolbar + hairline                             |   v
     *   +----------------------------------------+------------------+  ---
     *   |                                        |                  |   ^
     *   |           Requested viewport           |                  |   | requested
     *   |                                        |                  |   | viewport height
     *   +----------------------------------------+------------------+   v
     * </pre>
     *
     * <p>The caption bar insets of {@code sourceWindow} are used as a proxy for the ones the popup
     * window will get: {@code deltaWidthPx} is their left + right, {@code deltaHeightPx} their top
     * + bottom plus the browser top controls.
     *
     * @param sourceWindow The window the popup is opened from.
     * @param targetDisplay The display the popup will be opened on.
     * @param requestedViewportBoundsPx The requested viewport bounds in pixels of {@code
     *     targetDisplay}.
     * @return The predicted window bounds in pixels of {@code targetDisplay}.
     */
    @VisibleForTesting
    static Rect predictPopupWindowBounds(
            @Nullable WindowAndroid sourceWindow,
            DisplayAndroid targetDisplay,
            Rect requestedViewportBoundsPx) {
        final Rect windowBoundsPx = new Rect(requestedViewportBoundsPx);
        if (sPredictPopupWindowBoundsForTesting != null && !sPredictPopupWindowBoundsForTesting) {
            return windowBoundsPx;
        }
        if (sourceWindow == null) {
            Log.w(TAG, "predictPopupWindowBounds: sourceWindow is null -- bailing out");
            return windowBoundsPx;
        }

        final Context targetDisplayContext = targetDisplay.getWindowContext();
        if (targetDisplayContext == null) {
            return windowBoundsPx;
        }

        final InsetObserver insetObserver = sourceWindow.getInsetObserver();
        Insets captionBarInsetsOnSourceDisplay = Insets.NONE;
        if (insetObserver != null && insetObserver.getLastRawWindowInsets() != null) {
            captionBarInsetsOnSourceDisplay =
                    insetObserver
                            .getLastRawWindowInsets()
                            .getInsets(WindowInsetsCompat.Type.captionBar());
        }
        Log.v(
                TAG,
                "predictPopupWindowBounds: captionBarInsetsOnSourceDisplay = %s [px]",
                captionBarInsetsOnSourceDisplay);

        final float targetToSourceDisplayDensityRatio =
                targetDisplay.getDipScale() / sourceWindow.getDisplay().getDipScale();
        final int deltaWidthPx =
                Math.round(
                        (captionBarInsetsOnSourceDisplay.left
                                        + captionBarInsetsOnSourceDisplay.right)
                                * targetToSourceDisplayDensityRatio);
        final int deltaHeightPx =
                Math.round(
                                (captionBarInsetsOnSourceDisplay.top
                                                + captionBarInsetsOnSourceDisplay.bottom)
                                        * targetToSourceDisplayDensityRatio)
                        + predictBrowserTopControlsTotalHeightBelowTopInsetPx(
                                targetDisplayContext,
                                Math.round(
                                        captionBarInsetsOnSourceDisplay.top
                                                * targetToSourceDisplayDensityRatio));
        Log.v(
                TAG,
                "predictPopupWindowBounds: deltaWidthPx = %d px, deltaHeightPx = %d px",
                deltaWidthPx,
                deltaHeightPx);

        windowBoundsPx.right += deltaWidthPx;
        windowBoundsPx.bottom += deltaHeightPx;
        return windowBoundsPx;
    }

    /**
     * Returns the height in pixels of browser-owned popup UI elements visible below the top system
     * inset. For layouts that draw behind the status bar, this value represents the UI overflow
     * height; for standard layouts, it represents the full control container height.
     */
    private static int predictBrowserTopControlsTotalHeightBelowTopInsetPx(
            Context targetDisplayContext, int topInsetPx) {
        final int customTabsE2EHeaderHeightPx =
                DesktopPopupHeaderUtils.getFinalHeaderHeightPx(targetDisplayContext, topInsetPx);
        final int captionBarOverflowOverTopInsetPx = customTabsE2EHeaderHeightPx - topInsetPx;

        final int customTabsHeaderHeightPx =
                targetDisplayContext
                        .getResources()
                        .getDimensionPixelSize(R.dimen.custom_tabs_control_container_height);
        final int toolbarHairlineHeightPx =
                targetDisplayContext
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        final int totalTopControlsHeightPx =
                captionBarOverflowOverTopInsetPx
                        + customTabsHeaderHeightPx
                        + toolbarHairlineHeightPx;
        return totalTopControlsHeightPx;
    }

    public static void setReparentingTaskForTesting(ReparentingTask task) {
        sReparentingTaskForTesting = task;
        ResettersForTesting.register(() -> sReparentingTaskForTesting = null);
    }

    /**
     * If set to {@code false}, {@link #predictPopupWindowBounds} returns the requested viewport
     * bounds unchanged.
     */
    public static void setPredictPopupWindowBoundsForTesting(boolean predict) {
        sPredictPopupWindowBoundsForTesting = predict;
        ResettersForTesting.register(() -> sPredictPopupWindowBoundsForTesting = null);
    }

    public static void setMoveTabToNewPopupResultForTesting(boolean result) {
        sMoveTabToNewPopupResultForTesting = result;
        ResettersForTesting.register(() -> sMoveTabToNewPopupResultForTesting = null);
    }

    /**
     * If this is set to {@code false}, contextual popups will be launched without specifying the
     * {@link android.app.ActivityOptions#setMovableTaskRequired(boolean)} option.
     */
    public static void setSetMovableTaskRequiredForPopupsForTesting(
            Boolean setMovableTaskRequiredForPopups) {
        sSetMovableTaskRequiredForPopupsForTesting = setMovableTaskRequiredForPopups;
        ResettersForTesting.register(() -> sSetMovableTaskRequiredForPopupsForTesting = null);
    }

    @CalledByNativeForTesting
    public static void setTryStartActivityResultForTesting(boolean result) {
        sTryStartActivityResultForTesting = result;
        ResettersForTesting.register(() -> sTryStartActivityResultForTesting = null);
    }

    private static Intent createTrustedPopupIntent(
            @Nullable WindowFeatures windowFeatures,
            boolean isIncognitoBranded,
            @Nullable Bundle additionalIntentExtras) {
        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), CustomTabActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);
        if (windowFeatures != null) {
            intent.putExtra(EXTRA_REQUESTED_WINDOW_FEATURES, windowFeatures.toBundle());
        }
        if (additionalIntentExtras != null) {
            intent.putExtras(additionalIntentExtras);
        }
        if (isIncognitoBranded) {
            IncognitoCustomTabIntentDataProvider.addIncognitoExtrasForChromeFeatures(
                    intent, IncognitoCctCallerId.CONTEXTUAL_POPUP);
        }

        IntentUtils.addTrustedIntentExtras(intent);

        return intent;
    }

    private static Intent initializeDocumentPipIntent(
            WebContents webContents, PictureInPictureWindowOptions windowOptions) {
        Intent intent = new Intent();
        intent.setClass(
                ContextUtils.getApplicationContext(), DocumentPictureInPictureActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(DocumentPictureInPictureActivity.WEB_CONTENTS_KEY, webContents);
        intent.putExtra(
                DocumentPictureInPictureActivity.WINDOW_OPTIONS_KEY, windowOptions.toBundle());

        // Record the opener's origin at the time of the request to prevent origin spoofing
        // if the opener navigates before the Activity completes its launch.
        WebContents opener = webContents.getDocumentPictureInPictureOpener();
        if (opener != null) {
            intent.putExtra(
                    DocumentPictureInPictureActivity.INITIAL_OPENER_ORIGIN_KEY,
                    getOpenerOriginString(opener));
        }

        intent.setAction(Intent.ACTION_VIEW);

        return intent;
    }

    private static String getOpenerOriginString(WebContents opener) {
        RenderFrameHost frame = opener.getMainFrame();
        Origin origin =
                frame != null
                        ? frame.getLastCommittedOrigin()
                        : (opener.getLastCommittedUrl() != null
                                ? Origin.create(opener.getLastCommittedUrl())
                                : null);
        return (origin != null && !origin.isOpaque()) ? origin.toString() : "";
    }

    /**
     * Starts a popup through {@code activityStarter}, which must call {@link #tryStartActivity}
     * synchronously with the given options.
     */
    private boolean launchPopup(
            @PopupLaunchSource int launchSource,
            @PopupLaunchMode int launchMode,
            @Nullable Pair<DisplayAndroid, Rect> targetDisplayAndBounds,
            @Nullable WindowAndroid sourceWindow,
            ActivityStarter activityStarter) {
        Bundle options =
                buildPopupActivityOptions(targetDisplayAndBounds, sourceWindow, launchMode);
        // window.open falls back to a tab instead; native popups have none, so they retry.
        Bundle retryOptions =
                launchMode == PopupLaunchMode.MOVABLE_TASK_REQUIRED
                                && launchSource != PopupLaunchSource.FROM_TAB
                        ? buildPopupActivityOptions(
                                targetDisplayAndBounds,
                                sourceWindow,
                                PopupLaunchMode.MOVABLE_TASK_NOT_REQUIRED)
                        : null;
        mPendingPopupLaunch = new PendingPopupLaunch(launchSource, launchMode, retryOptions);
        try {
            return activityStarter.start(options);
        } finally {
            mPendingPopupLaunch = null;
        }
    }

    private static @Nullable Pair<DisplayAndroid, Rect> resolveTargetDisplayAndBounds(
            @Nullable WindowFeatures windowFeatures) {
        final Rect bounds = getWindowBoundsFromWindowFeatures(windowFeatures);
        if (bounds == null || bounds.isEmpty()) return null;
        return DisplayUtil.convertGlobalDipToLocalPxCoordinates(bounds);
    }

    /**
     * Returns the launch mode for popups without an opener tab.
     *
     * @see #getPopupLaunchMode(Tab, DisplayAndroid)
     */
    private static @PopupLaunchMode int getPopupLaunchMode() {
        if (sSetMovableTaskRequiredForPopupsForTesting != null
                && !sSetMovableTaskRequiredForPopupsForTesting) {
            return PopupLaunchMode.MOVABLE_TASK_NOT_REQUIRED;
        }
        return AndroidTaskUtils.canRepositionSelfWindows()
                ? PopupLaunchMode.MOVABLE_TASK_REQUIRED
                : PopupLaunchMode.MOVABLE_TASK_NOT_REQUIRED;
    }

    /** Returns the launch mode for a window.open popup opened from {@code openerTab}. */
    private static @PopupLaunchMode int getPopupLaunchMode(
            Tab openerTab, @Nullable DisplayAndroid targetDisplay) {
        if (sSetMovableTaskRequiredForPopupsForTesting != null
                && !sSetMovableTaskRequiredForPopupsForTesting) {
            return PopupLaunchMode.MOVABLE_TASK_NOT_REQUIRED;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.CINNAMON_BUN) {
            return PopupLaunchMode.FALLBACK_TO_TAB;
        }
        if (AndroidTaskUtils.canRepositionSelfWindows()) {
            return PopupLaunchMode.MOVABLE_TASK_REQUIRED;
        }
        // Without the permission, the OS only places the popup in freeform if the opener is a
        // freeform window on the same display.
        final WindowAndroid openerWindow = openerTab.getWindowAndroid();
        final boolean isOpenerWindowFreeform =
                openerWindow != null && isFreeformWindow(openerWindow);
        final boolean isOpenerWindowOnSameDisplay =
                openerWindow != null
                        && (targetDisplay == null
                                || targetDisplay.getDisplayId()
                                        == openerWindow.getDisplay().getDisplayId());
        if (isOpenerWindowFreeform && isOpenerWindowOnSameDisplay) {
            return PopupLaunchMode.MOVABLE_TASK_NOT_REQUIRED;
        }
        return PopupLaunchMode.FALLBACK_TO_TAB;
    }

    private static boolean isFreeformWindow(WindowAndroid window) {
        Activity activity = window.getActivity().get();
        if (activity == null) return false;
        ChromeAndroidTask task =
                ChromeAndroidTaskTrackerFactory.getInstance().get(activity.getTaskId());
        return task != null && task.isFreeformWindow();
    }

    private static @PopupLaunchOutcome int nonMovableTaskLaunchOutcome() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN
                ? PopupLaunchOutcome.NON_MOVABLE_TASK_LAUNCHED_NO_PERMISSION
                : PopupLaunchOutcome.NON_MOVABLE_TASK_LAUNCHED_BELOW_API37;
    }

    private static Bundle buildPopupActivityOptions(
            @Nullable Pair<DisplayAndroid, Rect> targetDisplayAndBounds,
            @Nullable WindowAndroid sourceWindow,
            @PopupLaunchMode int launchMode) {
        ActivityOptions activityOptions = ActivityOptions.makeBasic();

        if (launchMode == PopupLaunchMode.MOVABLE_TASK_REQUIRED) {
            // getPopupLaunchMode() only requires a movable task on API 37+; the SDK check is for
            // the NewApi lint.
            assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN) {
                activityOptions.setMovableTaskRequired(true);
            }
        }

        if (targetDisplayAndBounds != null) {
            final DisplayAndroid display = targetDisplayAndBounds.first;
            Rect bounds = targetDisplayAndBounds.second;

            bounds = predictPopupWindowBounds(sourceWindow, display, bounds);
            bounds = DisplayUtil.clampWindowToDisplay(bounds, display);

            activityOptions.setLaunchDisplayId(display.getDisplayId());
            activityOptions.setLaunchBounds(bounds);
        }

        return activityOptions.toBundle();
    }

    private static @Nullable ActivityOptions createDocumentPipActivityOptions(
            @Nullable Rect windowBounds) {
        // TODO(crbug.com/465413462): Add support for predicting final bounds.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.CINNAMON_BUN) {
            return null;
        }
        ActivityOptions activityOptions = ActivityOptions.makeBasic();
        activityOptions.setMovableTaskRequired(true);

        if (windowBounds != null && !windowBounds.isEmpty()) {
            final Pair<DisplayAndroid, Rect> localCoordinates =
                    DisplayUtil.convertGlobalDipToLocalPxCoordinates(windowBounds);
            if (localCoordinates != null) {
                final DisplayAndroid display = localCoordinates.first;
                Rect bounds = DisplayUtil.clampWindowToDisplay(localCoordinates.second, display);
                activityOptions.setLaunchDisplayId(display.getDisplayId());
                activityOptions.setLaunchBounds(bounds);
            }
        }

        return activityOptions;
    }

    private static void recordOutcome(
            @PopupLaunchSource @Nullable Integer launchSource, @PopupLaunchOutcome int outcome) {
        // Nothing is recorded for plain tab reparenting or under the test hook.
        if (launchSource == null
                || (sSetMovableTaskRequiredForPopupsForTesting != null
                        && !sSetMovableTaskRequiredForPopupsForTesting)) {
            return;
        }
        String variant =
                switch (launchSource) {
                    case PopupLaunchSource.NEW_POPUP -> "NewPopup";
                    case PopupLaunchSource.FROM_WEB_CONTENTS -> "FromWebContents";
                    case PopupLaunchSource.FROM_TAB -> "FromTab";
                    default -> null;
                };
        if (variant == null) return;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.MultiWindowMode.PopupLaunch.Outcome." + variant,
                outcome,
                PopupLaunchOutcome.COUNT);
    }

    private static @Nullable Rect getWindowBoundsFromWindowFeatures(
            @Nullable WindowFeatures windowFeatures) {
        if (windowFeatures == null
                || windowFeatures.width == null
                || windowFeatures.height == null) {
            return null;
        }

        final int widthDp = windowFeatures.width;
        final int heightDp = windowFeatures.height;
        if (widthDp <= 0 || heightDp <= 0) {
            return null;
        }
        final int leftDp = windowFeatures.left == null ? 0 : windowFeatures.left;
        final int topDp = windowFeatures.top == null ? 0 : windowFeatures.top;

        final int rightDp = leftDp + widthDp;
        final int bottomDp = topDp + heightDp;

        return new Rect(leftDp, topDp, rightDp, bottomDp);
    }

    private static ReparentingTask getReparentingTask(Tab tab) {
        if (sReparentingTaskForTesting != null) {
            return sReparentingTaskForTesting;
        }

        return ReparentingTask.from(tab);
    }

    private static int getDisplayIdFromTabId(int tabId) {
        final Tab tab = TabWindowManagerSingleton.getInstance().getTabById(tabId);
        if (tab == null) {
            return INVALID_DISPLAY;
        }
        final WindowAndroid window = tab.getWindowAndroid();
        if (window == null) {
            return INVALID_DISPLAY;
        }
        return window.getDisplay().getDisplayId();
    }
}
