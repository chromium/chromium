// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.util;

import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_SHORT_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Class containing utility functions for interacting with InfoBars at
 * a high level.
 */
public class VrInfoBarUtils {
    @IntDef({Button.PRIMARY, Button.SECONDARY})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Button {
        int PRIMARY = 0;
        int SECONDARY = 1;
    }

    /**
     * Determines whether InfoBars are present in the current activity.
     *
     * @param rule The ChromeActivityTestRule to get the InfoBars from.
     * @return True if there are any InfoBars present, false otherwise.
     */
    @SuppressWarnings("unchecked")
    public static boolean isInfoBarPresent(ChromeActivityTestRule rule) {
        List<InfoBar> infoBars = rule.getInfoBars();
        return infoBars != null && !infoBars.isEmpty();
    }

    /**
     * Clicks on either the primary or secondary button of the first InfoBar
     * in the activity.
     *
     * @param button Which button to click.
     * @param rule The ChromeActivityTestRule to get the InfoBars from.
     */
    @SuppressWarnings("unchecked")
    public static void clickInfoBarButton(final @Button int button, ChromeActivityTestRule rule) {
        if (!isInfoBarPresent(rule)) return;
        final List<InfoBar> infoBars = rule.getInfoBars();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            switch (button) {
                case Button.PRIMARY:
                    InfoBarUtil.clickPrimaryButton(infoBars.get(0));
                    break;
                default:
                    InfoBarUtil.clickSecondaryButton(infoBars.get(0));
            }
        });
        InfoBarUtil.waitUntilNoInfoBarsExist(rule.getInfoBars());
    }

    /**
     * Clicks on the close button of the first InfoBar in the activity.
     *
     * @param rule The ChromeActivityTestRule to get the InfoBars from.
     */
    @SuppressWarnings("unchecked")
    public static void clickInfobarCloseButton(ChromeActivityTestRule rule) {
        if (!isInfoBarPresent(rule)) return;
        final List<InfoBar> infoBars = rule.getInfoBars();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { InfoBarUtil.clickCloseButton(infoBars.get(0)); });
        InfoBarUtil.waitUntilNoInfoBarsExist(rule.getInfoBars());
    }

    /**
     * Determines is there is any InfoBar present in the given View hierarchy.
     *
     * @param rule The ChromeActivityTestRule to get the InfoBars from.
     * @param present Whether an InfoBar should be present.
     */
    public static void expectInfoBarPresent(
            final ChromeActivityTestRule rule, final boolean present) {
        CriteriaHelper.pollUiThread(()
                                            -> { return isInfoBarPresent(rule) == present; },
                "InfoBar bar did not " + (present ? "appear" : "disappear"), POLL_TIMEOUT_SHORT_MS,
                POLL_CHECK_INTERVAL_SHORT_MS);
    }
}
