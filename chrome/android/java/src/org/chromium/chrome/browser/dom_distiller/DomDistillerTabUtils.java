// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** A helper class for using the DOM Distiller. */
@JNINamespace("android")
@NullMarked
public class DomDistillerTabUtils {
    /** Triggering heuristics encoded in native enum DistillerHeuristicsType. */
    private static @Nullable Integer sHeuristics;

    /** Used to specify whether mobile friendly is enabled for testing purposes. */
    private static @Nullable Boolean sExcludeMobileFriendlyForTesting;

    @DistillerHeuristicsType private static @Nullable Integer sHeuristicsForTesting;

    private DomDistillerTabUtils() {}

    /**
     * Distills the given WebContents and waits for the result. If the distillation succeeds, then
     * the Viewer is opened via a navigation.
     *
     * @param webContents The WebContents to distill.
     * @param callback The callback which will be called upon success/failure of the distillation.
     */
    public static void distillCurrentPageAndViewIfSuccessful(
            WebContents webContents, Callback<Boolean> callback) {
        DomDistillerTabUtilsJni.get().distillCurrentPageAndViewIfSuccessful(webContents, callback);
    }

    /**
     * Starts distillation in the source {@link WebContents}. The viewer needs to be handled
     * elsewhere.
     *
     * @param webContents the WebContents to distill.
     */
    public static void distillCurrentPage(WebContents webContents) {
        DomDistillerTabUtilsJni.get().distillCurrentPage(webContents);
    }

    /**
     * Starts distillation in the source {@link WebContents} while navigating the destination
     * {@link WebContents} to view the distilled content. This does not take ownership of any
     * of the WebContents.
     *
     * @param sourceWebContents the WebContents to distill.
     * @param destinationWebContents the WebContents to display the distilled content in.
     */
    public static void distillAndView(
            WebContents sourceWebContents, WebContents destinationWebContents) {
        DomDistillerTabUtilsJni.get().distillAndView(sourceWebContents, destinationWebContents);
    }

    /**
     * Returns the formatted version of the original URL of a distillation, given the original URL.
     *
     * @param url The original URL.
     * @return the formatted URL of the original page.
     */
    public static String getFormattedUrlFromOriginalDistillerUrl(GURL url) {
        return DomDistillerTabUtilsJni.get().getFormattedUrlFromOriginalDistillerUrl(url);
    }

    /**
     * Detect if any heuristic is being used to determine if a page is distillable.
     * This is testing if the heuristic is not "NONE".
     *
     * @return True if heuristics are being used to detect distillable pages.
     */
    public static boolean isDistillerHeuristicsEnabled() {
        return getDistillerHeuristics() != DistillerHeuristicsType.NONE;
    }

    /**
     * Check if the distiller is reporting every page as distillable.
     *
     * @return True if heuristic is ALWAYS_TRUE.
     */
    public static boolean isHeuristicAlwaysTrue() {
        return getDistillerHeuristics() == DistillerHeuristicsType.ALWAYS_TRUE;
    }

    /** Returns whether the reader mode accessibility setting is enabled. */
    public static boolean isReaderModeAccessibilitySettingEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.READER_FOR_ACCESSIBILITY);
    }

    /**
     * Check if the distiller should report mobile-friendly pages as non-distillable.
     *
     * @return True if heuristic is ADABOOST_MODEL, and "Simplified view for accessibility" is
     *     disabled. Or false under certain experimental conditions.
     */
    public static boolean shouldExcludeMobileFriendly(Tab tab) {
        if (sExcludeMobileFriendlyForTesting != null) return sExcludeMobileFriendlyForTesting;
        // Including mobile-friendly by default only applies to the CPA, otherwise we fallback to
        // the accessibility setting.
        if (DomDistillerFeatures.triggerOnMobileFriendlyPages()
                && !ReaderModeManager.shouldUseReaderModeMessages(tab)) {
            return false;
        }

        return !isReaderModeAccessibilitySettingEnabled(tab.getProfile())
                && getDistillerHeuristics() == DistillerHeuristicsType.ADABOOST_MODEL;
    }

    public static void setExcludeMobileFriendlyForTesting(Boolean excludeForTesting) {
        sExcludeMobileFriendlyForTesting = excludeForTesting;
        ResettersForTesting.register(() -> sExcludeMobileFriendlyForTesting = null);
    }

    /** Set a test value of DistillerHeuristicsType. */
    public static void setDistillerHeuristicsForTesting(
            @DistillerHeuristicsType Integer distillerHeuristicsType) {
        sHeuristicsForTesting = distillerHeuristicsType;
        ResettersForTesting.register(() -> sHeuristicsForTesting = null);
    }

    /** Cached version of DomDistillerTabUtilsJni.get().getDistillerHeuristics(). */
    public static @DistillerHeuristicsType int getDistillerHeuristics() {
        if (sHeuristicsForTesting != null) {
            return sHeuristicsForTesting;
        }
        if (sHeuristics == null) {
            sHeuristics = DomDistillerTabUtilsJni.get().getDistillerHeuristics();
        }
        return sHeuristics;
    }

    /**
     * Set an InterceptNavigationDelegate on a WebContents.
     * @param delegate The navigation delegate.
     * @param webContents The WebContents to bind the delegate to.
     */
    public static void setInterceptNavigationDelegate(
            InterceptNavigationDelegate delegate, WebContents webContents) {
        DomDistillerTabUtilsJni.get().setInterceptNavigationDelegate(delegate, webContents);
    }

    /**
     * Runs distillability heuristics on the page to determine if it's suitable for reader mode.
     *
     * @param webContents The web contents to run the heuristic against.
     * @param callback The callback which informs the caller whether the given web contents are
     *     suitable for reader mode.
     */
    public static void runReadabilityHeuristicsOnWebContents(
            @Nullable WebContents webContents, Callback<Boolean> callback) {
        DomDistillerTabUtilsJni.get().runReadabilityHeuristicsOnWebContents(webContents, callback);
    }

    @NativeMethods
    public interface Natives {
        void distillCurrentPageAndViewIfSuccessful(
                WebContents webContents, Callback<Boolean> callback);

        void distillCurrentPage(WebContents webContents);

        void distillAndView(WebContents sourceWebContents, WebContents destinationWebContents);

        @JniType("std::u16string")
        String getFormattedUrlFromOriginalDistillerUrl(GURL url);

        int getDistillerHeuristics();

        void setInterceptNavigationDelegate(
                InterceptNavigationDelegate delegate, WebContents webContents);

        void runReadabilityHeuristicsOnWebContents(
                @Nullable WebContents webContents, Callback<Boolean> callback);
    }
}
