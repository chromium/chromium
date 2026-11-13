// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureList;
import org.chromium.base.MutableBooleanParamWithSafeDefault;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.lens.LensOverlayInvocationSource;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Utility methods for Contextual Tasks. */
@NullMarked
public final class ContextualTasksUtils {
    /** The host for the Contextual Tasks WebUI. */
    public static final String CONTEXTUAL_TASKS_HOST = "contextual-tasks";

    /**
     * Returns whether the given URL is a contextual tasks WebUI URL.
     *
     * @param gurl The URL to check.
     * @return True if it is a contextual tasks URL.
     */
    public static boolean isContextualTasksUrl(@Nullable GURL gurl) {
        if (GURL.isEmptyOrInvalid(gurl)) return false;
        return gurl.getScheme().equals(UrlConstants.CHROME_SCHEME)
                && gurl.getHost().equals(CONTEXTUAL_TASKS_HOST);
    }

    /**
     * Returns whether the Contextual Tasks UI is enabled. If called before native init, it will
     * look at the values of cached flags from earlier session. Always false on non-desktop form
     * factors (phones and tablets).
     *
     * @return True if Contextual Tasks UI is enabled.
     */
    public static boolean isContextualTasksUiEnabled() {
        if (!FeatureList.isNativeInitialized()) {
            return DeviceInfo.isDesktop()
                    && ChromeFeatureList.sContextualTasksSidePanel.isEnabled();
        }
        return ContextualTasksUtilsJni.get().isContextualTasksUiEnabled();
    }

    /**
     * Returns whether signed-out users (including incognito) are allowed to use Contextual Tasks in
     * the side panel on Desktop Android. Must be called after native initialization.
     *
     * @return True if kAllowSignedOutUserInDesktopAndroid is enabled on a desktop form factor.
     */
    public static boolean isAllowSignedOutUserInDesktopAndroidEnabled() {
        return ContextualTasksUtilsJni.get().isAllowSignedOutUserInDesktopAndroidEnabled();
    }

    /**
     * Returns whether the "Ask Google" contextual search entry points should be shown for the given
     * {@link Tab}.
     *
     * @param tab The current {@link Tab}, or null if unavailable.
     */
    public static boolean shouldShowAskGoogleEntryPoints(@Nullable Tab tab) {
        // Cached flag check avoids the JNI hop when the feature is off.
        if (!ChromeFeatureList.sContextualTasksEntryPoints.isEnabled()) return false;
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        return webContents != null
                && ContextualTasksUtilsJni.get().shouldShowAskGoogleEntryPoints(webContents);
    }

    /**
     * Returns whether a specific "Ask Google" contextual search surface entry point should be shown
     * for the given {@link Tab}.
     *
     * @param tab The current {@link Tab}, or null if unavailable.
     * @param surfaceParam The per-surface {@link MutableBooleanParamWithSafeDefault} parameter.
     */
    public static boolean shouldShowAskGoogleEntryPoint(
            @Nullable Tab tab, MutableBooleanParamWithSafeDefault surfaceParam) {
        return surfaceParam.getValue() && shouldShowAskGoogleEntryPoints(tab);
    }

    /**
     * Opens the Contextual Tasks side panel for the given {@link Tab}.
     *
     * @param tab The current {@link Tab}.
     * @param invocationSource The {@link LensOverlayInvocationSource} identifying the caller.
     */
    public static void openAskGoogleAboutThisPage(
            @Nullable Tab tab, @LensOverlayInvocationSource int invocationSource) {
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        if (webContents == null) return;
        ContextualTasksUtilsJni.get().openAskGoogleAboutThisPage(webContents, invocationSource);
    }

    /**
     * Toggles the Contextual Tasks side panel for the given {@link Tab}: closes the panel if it is
     * currently open for a contextual task, otherwise opens it via {@link
     * #openAskGoogleAboutThisPage}.
     *
     * @param tab The current {@link Tab}.
     * @param invocationSource The {@link LensOverlayInvocationSource} identifying the caller.
     */
    public static void toggleAskGoogleSidePanel(
            @Nullable Tab tab, @LensOverlayInvocationSource int invocationSource) {
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        if (webContents == null) return;
        ContextualTasksUtilsJni.get().toggleAskGoogleSidePanel(webContents, invocationSource);
    }

    /**
     * Fulfills a contextual omnibox suggestion with the given tab attached as context. On Desktop
     * Android the AI Mode answer is shown in the Contextual Tasks side panel; when the contextual
     * flow is unavailable the suggestion URL is loaded in the tab instead.
     *
     * @param webContents The {@link WebContents} of the tab the suggestion was issued for.
     * @param suggestionUrl The search URL of the contextual suggestion.
     */
    public static void issueOmniboxContextualQuery(WebContents webContents, GURL suggestionUrl) {
        ContextualTasksUtilsJni.get().issueOmniboxContextualQuery(webContents, suggestionUrl);
    }

    @NativeMethods
    public interface Natives {
        boolean isContextualTasksUiEnabled();

        boolean isAllowSignedOutUserInDesktopAndroidEnabled();

        boolean shouldShowAskGoogleEntryPoints(
                @JniType("content::WebContents*") WebContents webContents);

        void openAskGoogleAboutThisPage(
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("lens::LensOverlayInvocationSource") @LensOverlayInvocationSource
                        int invocationSource);

        void toggleAskGoogleSidePanel(
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("lens::LensOverlayInvocationSource") @LensOverlayInvocationSource
                        int invocationSource);

        void issueOmniboxContextualQuery(
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("GURL") GURL suggestionUrl);
    }
}
