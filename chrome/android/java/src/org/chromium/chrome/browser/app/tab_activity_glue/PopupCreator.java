// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tab_activity_glue;

import static android.view.Display.INVALID_DISPLAY;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.ActivityManager.AppTask;
import android.app.ActivityOptions;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.AndroidRuntimeException;
import android.util.Pair;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.IncognitoCctCallerId;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.PopupIntentCreator;
import org.chromium.chrome.browser.customtabs.PopupIntentCreatorProvider;
import org.chromium.chrome.browser.customtabs.features.desktop_popup_header.DesktopPopupHeaderUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.media.DocumentPictureInPictureActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.AndroidTaskUtils;
import org.chromium.chrome.browser.util.PictureInPictureWindowOptions;
import org.chromium.chrome.browser.util.WindowFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.WindowInsetsUtils;

/** Handles launching new popup windows as CCTs and Document Picture-in-Picture windows. */
@NullMarked
public class PopupCreator implements PopupIntentCreator {
    public static final String EXTRA_REQUESTED_WINDOW_FEATURES =
            "chrome.browser.app.tab_activity_glue.PopupCreator.EXTRA_REQUESTED_WINDOW_FEATURES";

    private static final String TAG = "PopupCreator";
    private static @Nullable ReparentingTask sReparentingTaskForTesting;
    private static @Nullable Insets sInsetsForecastForTesting;
    private static @Nullable Boolean sMoveTabToNewPopupResultForTesting;
    private static @Nullable Boolean sMoveToNewDocumentPiPWindowResultForTesting;
    private static @Nullable Boolean sSetMovableTaskRequiredForPopupsForTesting;

    /**
     * Initializes {@link PopupIntentCreator} with top-level dependencies so lower-level code can
     * obtain a popup Intent via {@link PopupIntentCreator#createPopupIntent}.
     */
    public static void initializePopupIntentCreator() {
        if (PopupIntentCreatorProvider.getInstance() != null) {
            return;
        }
        PopupIntentCreatorProvider.setInstance(new PopupCreator());
    }

    @Override
    public Intent createPopupIntent(@Nullable WindowFeatures windowFeatures, boolean isIncognito) {
        final Intent intent = initializePopupIntent();
        if (windowFeatures != null) {
            intent.putExtra(EXTRA_REQUESTED_WINDOW_FEATURES, windowFeatures.toBundle());
        }
        if (isIncognito) {
            IncognitoCustomTabIntentDataProvider.addIncognitoExtrasForChromeFeatures(
                    intent, IncognitoCctCallerId.CONTEXTUAL_POPUP);
        }
        return intent;
    }

    /**
     * Moves the given {@link Tab} to a new Custom Tab popup window.
     *
     * @param tab The {@link Tab} to move.
     * @param windowFeatures The {@link WindowFeatures} to use for the new Custom Tab popup window.
     * @return {@code true} if the tab was successfully reparented to a new movable Task, {@code
     *     false} otherwise
     */
    public static boolean moveTabToNewPopup(Tab tab, WindowFeatures windowFeatures) {
        if (sMoveTabToNewPopupResultForTesting != null) {
            return sMoveTabToNewPopupResultForTesting;
        }

        final Rect windowBounds = getWindowBoundsFromWindowFeatures(windowFeatures);
        final ActivityOptions activityOptions =
                createPopupActivityOptions(windowBounds, tab.getWindowAndroid());
        if (activityOptions == null) {
            return false;
        }

        initializePopupIntentCreator();
        final PopupIntentCreator popupIntentCreator =
                assertNonNull(PopupIntentCreatorProvider.getInstance());
        final Intent intent =
                popupIntentCreator.createPopupIntent(windowFeatures, tab.isIncognitoBranded());

        return getReparentingTask(tab)
                .begin(tab.getContext(), intent, activityOptions.toBundle(), null);
    }

    /**
     * Moves the given {@link WebContents} to a new Document Picture-in-Picture window.
     *
     * @param webContents The {@link WebContents} to move.
     * @param windowOptions The {@link PictureInPictureWindowOptions} to use for the new Document
     *     Picture-in-Picture window.
     */
    public static boolean moveWebContentsToNewDocumentPictureInPictureWindow(
            WebContents webContents, PictureInPictureWindowOptions windowOptions) {
        if (sMoveToNewDocumentPiPWindowResultForTesting != null) {
            return sMoveToNewDocumentPiPWindowResultForTesting;
        }

        final ActivityOptions activityOptions =
                createDocumentPipActivityOptions(
                        windowOptions.windowBounds, webContents.getTopLevelNativeWindow());
        if (activityOptions == null) {
            return false;
        }

        final Intent intent = initializeDocumentPipIntent(webContents, windowOptions);
        return tryStartActivity(
                ContextUtils.getApplicationContext(), intent, activityOptions.toBundle());
    }

    /**
     * Adjusts window bounds of given {@link ChromeActivity} to match requested {@link
     * WindowFeatures} using at most one {@link android.app.ActivityManager.AppTask#moveTaskTo} call
     * on best-effort basis.
     */
    public static void adjustWindowBoundsToRequested(
            ChromeActivity activity, @Nullable WindowFeatures requestedWindowFeaturesDp) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.BAKLAVA) {
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
                "adjustWindowBounds: current viewport size = ("
                        + realViewportWidthDp
                        + " x "
                        + realViewportHeightDp
                        + ") [dp]");
        Log.v(TAG, "adjustWindowBounds: top-level window bounds = " + realWindowBoundsPx + " [px]");
        Log.v(
                TAG,
                "adjustWindowBounds: requested WindowFeatures = "
                        + requestedWindowFeaturesDp
                        + " [dp]");

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

            Log.v(TAG, "adjustWindowBounds: widthDiffDp = " + widthDiffDp + " dp");
            Log.v(TAG, "adjustWindowBounds: widthDiffPx = " + widthDiffPx + " px");
            Log.v(TAG, "adjustWindowBounds: heightDiffDp = " + heightDiffDp + " dp");
            Log.v(TAG, "adjustWindowBounds: heightDiffPx = " + heightDiffPx + " px");
            Log.v(TAG, "adjustWindowBounds: display.getDipScale() = " + display.getDipScale());

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
                    "adjustWindowBounds: current window insets = "
                            + windowInsetsOnSourceDisplay
                            + " [px]");
            Log.v(
                    TAG,
                    "adjustWindowBounds: getTopControlsHeight from activity's BCM = "
                            + activity.getBrowserControlsManager().getTopControlsHeight()
                            + " px");
            Log.v(
                    TAG,
                    "adjustWindowBounds: getTopControlsHairlineHeight from activity's BCM = "
                            + activity.getBrowserControlsManager().getTopControlsHairlineHeight()
                            + " px");
        }

        if (realWindowBoundsPx.equals(targetWindowBoundsPx)) {
            Log.v(TAG, "adjustWindowBounds: perfect match -- no repositioning required");
            return;
        }
        Log.v(
                TAG,
                "adjustWindowBounds: repositioning required -- current bounds = "
                        + realWindowBoundsPx
                        + ", target bounds = "
                        + targetWindowBoundsPx);

        final AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
        if (delegate == null) {
            Log.w(TAG, "adjustWindowBounds: AconfigFlaggedApiDelegate is null -- bailing out");
            return;
        }

        final AppTask appTask = AndroidTaskUtils.getAppTaskFromId(activity, activity.getTaskId());
        if (appTask == null) {
            Log.w(TAG, "adjustWindowBounds: cannot find the AppTask -- bailing out");
            return;
        }

        Log.v(TAG, "adjustWindowBounds: dispatching the moveTaskTo call");
        delegate.moveTaskTo(appTask, INVALID_DISPLAY /* current display */, targetWindowBoundsPx);
    }

    /**
     * Returns negative insets expected to be the difference between WebContents viewport and
     * system-level window of the browser.
     */
    public static Insets getPopupInsetsForecast(
            @Nullable WindowAndroid sourceWindow, DisplayAndroid targetDisplay) {
        if (sInsetsForecastForTesting != null) {
            return sInsetsForecastForTesting;
        }
        if (sourceWindow == null) {
            Log.w(TAG, "getPopupInsetsForecast: sourceWindow is null -- bailing out");
            return Insets.NONE;
        }

        final Context targetDisplayContext = targetDisplay.getWindowContext();
        if (targetDisplayContext == null) {
            return Insets.NONE;
        }

        final InsetObserver insetObserver = sourceWindow.getInsetObserver();
        Insets windowInsetsOnSourceDisplay = Insets.NONE;
        if (insetObserver != null && insetObserver.getLastRawWindowInsets() != null) {
            windowInsetsOnSourceDisplay =
                    insetObserver
                            .getLastRawWindowInsets()
                            .getInsets(WindowInsetsCompat.Type.captionBar());
        }
        Log.v(
                TAG,
                "getPopupInsetsForecast: windowInsetsOnSourceDisplay = "
                        + windowInsetsOnSourceDisplay
                        + " [px]");

        final float densityFactor =
                targetDisplay.getDipScale() / sourceWindow.getDisplay().getDipScale();
        final Insets forecastedWindowInsetsOnTargetDisplay =
                Insets.of(
                        0,
                        0,
                        Math.round(
                                (windowInsetsOnSourceDisplay.left
                                                + windowInsetsOnSourceDisplay.right)
                                        * densityFactor),
                        Math.round(
                                (windowInsetsOnSourceDisplay.top
                                                + windowInsetsOnSourceDisplay.bottom)
                                        * densityFactor));
        Log.v(
                TAG,
                "getPopupInsetsForecast: forecastedWindowInsetsOnTargetDisplay = "
                        + forecastedWindowInsetsOnTargetDisplay
                        + " [px]");

        final int totalTopControlsHeightPx =
                predictBrowserTopControlsTotalHeightBelowTopInsetPx(
                        targetDisplayContext,
                        Math.round(windowInsetsOnSourceDisplay.top * densityFactor));
        final Insets totalInsets =
                Insets.add(
                        forecastedWindowInsetsOnTargetDisplay,
                        Insets.of(0, 0, 0, totalTopControlsHeightPx));
        Log.v(TAG, "getPopupInsetsForecast: totalInsets = " + totalInsets + " [px]");
        final Insets invertedTotalInsets = Insets.subtract(Insets.NONE, totalInsets);
        return invertedTotalInsets;
    }

    /**
     * Returns the height in pixels of browser-owned popup UI elements visible below the top system
     * inset. For layouts that draw behind the status bar, this value represents the UI overflow
     * height; for standard layouts, it represents the full control container height.
     */
    private static int predictBrowserTopControlsTotalHeightBelowTopInsetPx(
            Context targetDisplayContext, int topInsetPx) {
        // Without edge-to-edge drawing the caption bar is precisely the top inset.
        int captionBarOverflowOverTopInsetPx = 0;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_WINDOW_POPUP_CUSTOM_TAB_UI)) {
            final int customTabsE2EHeaderHeightPx =
                    DesktopPopupHeaderUtils.getFinalHeaderHeightPx(
                            targetDisplayContext, topInsetPx);
            Log.v(
                    TAG,
                    "predictBrowserTopControlsTotalHeightPx: customTabsE2EHeaderHeightPx = "
                            + customTabsE2EHeaderHeightPx
                            + " px");
            Log.v(
                    TAG,
                    "predictBrowserTopControlsTotalHeightPx: topInsetPx = " + topInsetPx + " px");
            captionBarOverflowOverTopInsetPx = customTabsE2EHeaderHeightPx - topInsetPx;
        }

        final int customTabsHeaderHeightPx =
                targetDisplayContext
                        .getResources()
                        .getDimensionPixelSize(R.dimen.custom_tabs_control_container_height);
        final int toolbarHairlineHeightPx =
                targetDisplayContext
                        .getResources()
                        .getDimensionPixelSize(R.dimen.toolbar_hairline_height);
        Log.v(
                TAG,
                "predictBrowserTopControlsTotalHeightPx: captionBarOverflowOverTopInsetPx = "
                        + captionBarOverflowOverTopInsetPx
                        + " px");
        Log.v(
                TAG,
                "predictBrowserTopControlsTotalHeightPx: customTabsHeaderHeightPx = "
                        + customTabsHeaderHeightPx
                        + " px");
        Log.v(
                TAG,
                "predictBrowserTopControlsTotalHeightPx: toolbarHairlineHeightPx = "
                        + toolbarHairlineHeightPx
                        + " px");
        final int totalTopControlsHeightPx =
                captionBarOverflowOverTopInsetPx
                        + customTabsHeaderHeightPx
                        + toolbarHairlineHeightPx;
        Log.v(
                TAG,
                "predictBrowserTopControlsTotalHeightPx: totalTopControlsHeightPx = "
                        + totalTopControlsHeightPx
                        + " px");
        return totalTopControlsHeightPx;
    }

    /**
     * Starts an activity using given {@link android.content.Context}, {@link
     * android.content.Intent}, and {@link android.os.Bundle} of ActivityOptions. Catches exceptions
     * likely to be thrown when {@link android.app.ActivityOptions#setMovableTaskRequired(boolean)}
     * is set to {@code true} in the {@link android.app.ActivityOptions} object represented by the
     * {@link android.os.Bundle} provided.
     *
     * @param context The Context on which the {@link
     *     android.content.Context#startActivity(android.content.Intent, android.os.Bundle)} method
     *     will be executed.
     * @param intent The Intent passed to the {@code startActivity} call.
     * @param activityOptions The Bundle passed to the {@code startActivity} call.
     * @return {@code true} if succeeded, {@code false} otherwise.
     * @see android.app.ActivityOptions#toBundle()
     * @see android.app.ActivityOptions#setMovableTaskRequired(boolean)
     */
    public static boolean tryStartActivity(
            Context context, Intent intent, @Nullable Bundle activityOptions) {
        try {
            context.startActivity(intent, activityOptions);
        } catch (SecurityException e) {
            Log.w(TAG, "tryStartActivity: no permission to start a movable task", e);
            return false;
        } catch (AndroidRuntimeException e) {
            final AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
            if (delegate == null) {
                Log.w(TAG, "tryStartActivity: AconfigFlaggedApiDelegate is null");
                return false;
            }

            if (delegate.isInfeasibleActivityOptionsException(e)) {
                Log.w(
                        TAG,
                        "tryStartActivity: startActivity threw InfeasibleActivityOptionsException");
                return false;
            }

            throw e;
        }

        return true;
    }

    public static void setReparentingTaskForTesting(ReparentingTask task) {
        sReparentingTaskForTesting = task;
        ResettersForTesting.register(() -> sReparentingTaskForTesting = null);
    }

    /**
     * Overrides values returned by {@link getPopupInsetsForecast}. Usually it is desired to pass
     * insets that are non-positive in all directions.
     */
    public static void setInsetsForecastForTesting(Insets insets) {
        sInsetsForecastForTesting = insets;
        ResettersForTesting.register(() -> sInsetsForecastForTesting = null);
    }

    /** Overrides values returned by {@link moveTabToNewPopup}. */
    public static void setMoveTabToNewPopupResultForTesting(Boolean moveTabToNewPopupResult) {
        sMoveTabToNewPopupResultForTesting = moveTabToNewPopupResult;
        ResettersForTesting.register(() -> sMoveTabToNewPopupResultForTesting = null);
    }

    /** Overrides values returned by {@link moveWebContentsToNewDocumentPictureInPictureWindow}. */
    public static void setMoveToNewDocumentPiPWindowResultForTesting(
            Boolean moveToNewDocumentPiPWindowResult) {
        sMoveToNewDocumentPiPWindowResultForTesting = moveToNewDocumentPiPWindowResult;
        ResettersForTesting.register(() -> sMoveToNewDocumentPiPWindowResultForTesting = null);
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

    private static Intent initializePopupIntent() {
        Intent intent = new Intent();
        intent.setClass(ContextUtils.getApplicationContext(), CustomTabActivity.class);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.POPUP);

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

        intent.setAction(Intent.ACTION_VIEW);

        return intent;
    }

    private static @Nullable ActivityOptions createPopupActivityOptions(
            @Nullable Rect windowBounds, @Nullable WindowAndroid sourceWindow) {
        return createPopupActivityOptions(
                windowBounds,
                sourceWindow,
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ANDROID_WINDOW_POPUP_PREDICT_FINAL_BOUNDS));
    }

    private static @Nullable ActivityOptions createDocumentPipActivityOptions(
            @Nullable Rect windowBounds, @Nullable WindowAndroid sourceWindow) {
        // TODO(crbug.com/465413462): Add support for predicting final bounds.
        return createPopupActivityOptions(
                windowBounds, sourceWindow, /* predictFinalBounds= */ false);
    }

    private static @Nullable ActivityOptions createPopupActivityOptions(
            @Nullable Rect windowBounds,
            @Nullable WindowAndroid sourceWindow,
            boolean predictFinalBounds) {
        ActivityOptions activityOptions = ActivityOptions.makeBasic();

        if (sSetMovableTaskRequiredForPopupsForTesting == null
                || sSetMovableTaskRequiredForPopupsForTesting) {
            final AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
            if (delegate == null) {
                Log.w(TAG, "createPopupActivityOptions: AconfigFlaggedApiDelegate is null");
                return null;
            }

            activityOptions = delegate.setMovableTaskRequired(activityOptions);
            if (activityOptions == null) {
                return null;
            }
        }

        if (windowBounds == null) {
            return activityOptions;
        }

        final Pair<DisplayAndroid, Rect> localCoordinates =
                DisplayUtil.convertGlobalDipToLocalPxCoordinates(windowBounds);

        if (localCoordinates != null) {
            final DisplayAndroid display = localCoordinates.first;
            Rect bounds = localCoordinates.second;

            Log.v(TAG, "createPopupActivityOptions: ideal bounds = " + bounds);

            if (predictFinalBounds) {
                Insets insets = getPopupInsetsForecast(sourceWindow, display);
                Log.v(TAG, "createPopupActivityOptions: apply insets = " + insets);
                bounds = WindowInsetsUtils.insetRectangle(bounds, insets);
            }

            bounds = DisplayUtil.clampWindowToDisplay(bounds, display);
            Log.v(TAG, "createPopupActivityOptions: clamped bounds = " + bounds);

            activityOptions.setLaunchDisplayId(display.getDisplayId());
            activityOptions.setLaunchBounds(bounds);
        }

        return activityOptions;
    }

    private static @Nullable Rect getWindowBoundsFromWindowFeatures(WindowFeatures windowFeatures) {
        if (windowFeatures.width == null || windowFeatures.height == null) {
            return null;
        }

        final int widthDp = windowFeatures.width;
        final int heightDp = windowFeatures.height;
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
