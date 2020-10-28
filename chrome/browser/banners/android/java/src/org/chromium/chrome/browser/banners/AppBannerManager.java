// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Function;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manages an AppBannerInfoBar for a Tab.
 *
 * The AppBannerManager is responsible for fetching details about native apps to display in the
 * banner. The actual observation of the WebContents (which triggers the automatic creation and
 * removal of banners, among other things) is done by the native-side AppBannerManagerAndroid.
 */
@JNINamespace("banners")
public class AppBannerManager {
    /**
     * A struct containing the string resources IDs for the strings to show in the install
     * dialog (both the dialog title and the accept button).
     */
    public static class InstallStringPair {
        public final @StringRes int titleTextId;
        public final @StringRes int buttonTextId;

        public InstallStringPair(@StringRes int titleText, @StringRes int buttonText) {
            titleTextId = titleText;
            buttonTextId = buttonText;
        }
    }

    public static final InstallStringPair PWA_PAIR = new InstallStringPair(
            R.string.menu_add_to_homescreen_install, R.string.app_banner_install);
    public static final InstallStringPair NON_PWA_PAIR =
            new InstallStringPair(R.string.menu_add_to_homescreen, R.string.add);

    /** The key to use to store and retrieve (from the menu data) what was shown in the menu. */
    public static final String MENU_TITLE_KEY = "AppMenuTitleShown";

    private static final String TAG = "AppBannerManager";

    /** Retrieves information about a given package. */
    private static AppDetailsDelegate sAppDetailsDelegate;

    private static Function<Profile, Tracker> sTrackerFromProfileFactory;

    /** Pointer to the native side AppBannerManager. */
    private long mNativePointer;

    /**
     * Sets the delegate that provides information about a given package.
     * @param delegate Delegate to use.  Previously set ones are destroyed.
     */
    public static void setAppDetailsDelegate(AppDetailsDelegate delegate) {
        if (sAppDetailsDelegate != null) sAppDetailsDelegate.destroy();
        sAppDetailsDelegate = delegate;
    }

    /**
     * Sets the factory to obtain a Tracker from.
     * @param trackerFromProfileFactory The factory to use.
     */
    public static void setTrackerFromProfileFactory(
            Function<Profile, Tracker> trackerFromProfileFactory) {
        sTrackerFromProfileFactory = trackerFromProfileFactory;
    }

    /**
     * Constructs an AppBannerManager.
     * @param nativePointer the native-side object that owns this AppBannerManager.
     */
    private AppBannerManager(long nativePointer) {
        mNativePointer = nativePointer;
    }

    @CalledByNative
    private static AppBannerManager create(long nativePointer) {
        return new AppBannerManager(nativePointer);
    }

    @CalledByNative
    private void destroy() {
        mNativePointer = 0;
    }

    /**
     * Grabs package information for the banner asynchronously.
     * @param url         URL for the page that is triggering the banner.
     * @param packageName Name of the package that is being advertised.
     */
    @CalledByNative
    private void fetchAppDetails(
            String url, String packageName, String referrer, int iconSizeInDp) {
        if (sAppDetailsDelegate == null) return;

        Context context = ContextUtils.getApplicationContext();
        int iconSizeInPx =
                Math.round(context.getResources().getDisplayMetrics().density * iconSizeInDp);
        sAppDetailsDelegate.getAppDetailsAsynchronously(
                createAppDetailsObserver(), url, packageName, referrer, iconSizeInPx);
    }

    /**
     * Request to show the in-product help for installing a PWA.
     * @param webContents The current WebContents.
     * @return An error message, if unsuccessful. Blank if the request was made.
     */
    @CalledByNative
    private String showInProductHelp(WebContents webContents) {
        // Consult the tracker to see if the IPH can be shown.
        final Tracker tracker =
                sTrackerFromProfileFactory.apply(Profile.fromWebContents(webContents));
        if (!tracker.wouldTriggerHelpUI(FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE)) {
            // Tracker replied that the request to show will not be honored. Return whether the
            // limit of how often to show has been exceeded.
            return "Trigger state: "
                    + tracker.getTriggerState(FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE);
        }

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return "No window";
        AppBannerInProductHelpController controller =
                AppBannerInProductHelpControllerProvider.from(window);
        if (controller == null) return "No controller";
        controller.requestInProductHelp();
        return "";
    }

    private AppDetailsDelegate.Observer createAppDetailsObserver() {
        return new AppDetailsDelegate.Observer() {
            /**
             * Called when data about the package has been retrieved, which includes the url for the
             * app's icon but not the icon Bitmap itself.
             * @param data Data about the app.  Null if the task failed.
             */
            @Override
            public void onAppDetailsRetrieved(AppData data) {
                if (data == null || mNativePointer == 0) return;

                String imageUrl = data.imageUrl();
                if (TextUtils.isEmpty(imageUrl)) return;

                AppBannerManagerJni.get().onAppDetailsRetrieved(mNativePointer,
                        AppBannerManager.this, data, data.title(), data.packageName(),
                        data.imageUrl());
            }
        };
    }

    /** Returns the language option to use for the add to homescreen dialog and menu item. */
    public static InstallStringPair getHomescreenLanguageOption(Tab currentTab) {
        AppBannerManager manager = currentTab != null ? AppBannerManager.forTab(currentTab) : null;
        if (manager != null && manager.getIsPwa(currentTab)) {
            return PWA_PAIR;
        } else {
            return NON_PWA_PAIR;
        }
    }

    /** Sets the app-banner-showing logic to ignore the Chrome channel. */
    @VisibleForTesting
    public static void ignoreChromeChannelForTesting() {
        AppBannerManagerJni.get().ignoreChromeChannelForTesting();
    }

    /** Returns whether the native AppBannerManager is working. */
    @VisibleForTesting
    public boolean isRunningForTesting() {
        return AppBannerManagerJni.get().isRunningForTesting(mNativePointer, AppBannerManager.this);
    }

    /** Sets constants (in days) the banner should be blocked for after dismissing and ignoring. */
    @VisibleForTesting
    static void setDaysAfterDismissAndIgnoreForTesting(int dismissDays, int ignoreDays) {
        AppBannerManagerJni.get().setDaysAfterDismissAndIgnoreToTrigger(dismissDays, ignoreDays);
    }

    /** Sets a constant (in days) that gets added to the time when the current time is requested. */
    @VisibleForTesting
    static void setTimeDeltaForTesting(int days) {
        AppBannerManagerJni.get().setTimeDeltaForTesting(days);
    }

    /** Sets the total required engagement to trigger the banner. */
    @VisibleForTesting
    static void setTotalEngagementForTesting(double engagement) {
        AppBannerManagerJni.get().setTotalEngagementToTrigger(engagement);
    }

    /** Returns the AppBannerManager object. This is owned by the C++ banner manager. */
    public static AppBannerManager forTab(Tab tab) {
        ThreadUtils.assertOnUiThread();
        return AppBannerManagerJni.get().getJavaBannerManagerForWebContents(tab.getWebContents());
    }

    /**
     * Checks whether the tab has navigated to a PWA.
     * @param tab The tab to check.
     * @return true if the tab has been determined to contain a PWA.
     */
    public boolean getIsPwa(Tab tab) {
        return !TextUtils.equals(
                "", AppBannerManagerJni.get().getInstallableWebAppName(tab.getWebContents()));
    }

    @NativeMethods
    interface Natives {
        AppBannerManager getJavaBannerManagerForWebContents(WebContents webContents);
        String getInstallableWebAppName(WebContents webContents);
        boolean onAppDetailsRetrieved(long nativeAppBannerManagerAndroid, AppBannerManager caller,
                AppData data, String title, String packageName, String imageUrl);
        // Testing methods.
        void ignoreChromeChannelForTesting();
        boolean isRunningForTesting(long nativeAppBannerManagerAndroid, AppBannerManager caller);
        void setDaysAfterDismissAndIgnoreToTrigger(int dismissDays, int ignoreDays);
        void setTimeDeltaForTesting(int days);
        void setTotalEngagementToTrigger(double engagement);
    }
}
