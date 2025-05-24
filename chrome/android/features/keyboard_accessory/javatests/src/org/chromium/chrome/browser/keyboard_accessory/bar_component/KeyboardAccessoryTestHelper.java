// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.View;

import org.hamcrest.Matchers;

import org.chromium.base.test.util.Criteria;
import org.chromium.chrome.browser.keyboard_accessory.R;

/** Helpers in this class simplify interactions with the Keyboard Accessory bar component. */
public class KeyboardAccessoryTestHelper {
    /**
     * Returns true if the accessory becomes visible. It is not guaranteed that the view reflects
     * this state already. Use {@link #checkThatAccessoryViewFullyShown(Activity)} for that.
     *
     * @param accessory An {@link KeyboardAccessoryCoordinator}
     * @return True iff the component was ordered to show.
     */
    public static boolean accessoryStartedShowing(KeyboardAccessoryCoordinator accessory) {
        return accessory != null && accessory.isShown();
    }

    /**
     * Returns true if the accessory starts hiding. It is not guaranteed that the view reflects this
     * state already. Use {@link #accessoryViewFullyHidden(Activity)} (Activity)} for that.
     *
     * @param accessory An {@link KeyboardAccessoryCoordinator}
     * @return True iff the component was ordered to hide.
     */
    public static boolean accessoryStartedHiding(KeyboardAccessoryCoordinator accessory) {
        return accessory != null && !accessory.isShown();
    }

    /**
     * Helper that finds the accessory bar and checks whether it's shown.
     *
     * <p>For use with {@link org.chromium.base.test.util.CriteriaHelper}.
     *
     * @param activity The {@link Activity} containing the accessory bar.
     */
    public static void checkThatAccessoryViewFullyShown(Activity activity) {
        KeyboardAccessoryView accessory = activity.findViewById(R.id.keyboard_accessory);
        Criteria.checkThat("Null accessory view", accessory, Matchers.notNullValue());
        Criteria.checkThat("isShown() returning false", accessory.isShown(), Matchers.is(true));
        Criteria.checkThat(
                "Accessory has running animations",
                accessory.hasRunningAnimation(),
                Matchers.is(false));
        Criteria.checkThat(
                "View not fully on screen", isViewOnScreen(accessory), Matchers.is(true));
    }

    private static boolean isViewOnScreen(View target) {
        if (!target.isShown()) {
            return false;
        }
        final Rect actualPosition = new Rect();
        final boolean isGlobalVisible = target.getGlobalVisibleRect(actualPosition);
        final Rect screen =
                new Rect(
                        0,
                        0,
                        Resources.getSystem().getDisplayMetrics().widthPixels,
                        Resources.getSystem().getDisplayMetrics().heightPixels);
        return isGlobalVisible && Rect.intersects(actualPosition, screen);
    }

    /**
     * Helper that finds the accessory bar and checks whether it's hidden. Returns false until
     * animations have concluded.
     *
     * @param activity The {@link Activity} containing the accessory bar.
     * @return True iff the bar view is hidden and animations have ended.
     */
    public static boolean accessoryViewFullyHidden(Activity activity) {
        KeyboardAccessoryView accessory = activity.findViewById(R.id.keyboard_accessory);
        return accessory == null || (!accessory.isShown() && !accessory.hasRunningAnimation());
    }
}
