// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.os.CancellationSignal;

import org.hamcrest.Matchers;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;

/**
 * Utilities for writing tests that check for or perform direct actions on an activity.
 */
class DirectActionTestUtils {
    /** Perform a direct action, with the given name. */
    public static void callOnPerformDirectActions(
            ChromeActivity activity, String actionId, Callback<Bundle> callback) {
        // This method is not taking a Consumer to avoid issues with tests running against Android
        // API < 24 not even being able to load the test class.

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.onPerformDirectAction(actionId, Bundle.EMPTY, new CancellationSignal(),
                    (r) -> callback.onResult((Bundle) r));
        });
    }

    /** Gets the list of direct actions. */
    public static List<String> callOnGetDirectActions(ChromeActivity activity) {
        List<String> directActions = new ArrayList<>();

        // onGetDirectActions reports a List<String> because that's what FakeDirectActionReporter
        // creates.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.onGetDirectActions(new CancellationSignal(),
                    (actions) -> directActions.addAll((List<String>) actions));
        });
        return directActions;
    }

    /**
     * Sets the ChromeActivityTestRule by forcing "go_forward" to be available.
     *
     * <p>The activity of the given rule must have been started and have loaded a page.
     */
    static List<String> setupActivityAndGetDirectAction(ChromeActivityTestRule<?> rule)
            throws Exception {
        allowGoForward(rule);
        return callOnGetDirectActions(rule.getActivity());
    }

    /**
     * Forces availability of the "go_forward" direct action on the current tab by loading another
     * URL then navigating back to the current one.
     *
     * <p>The activity of the given rule must have been started and have loaded a page.
     */
    static void allowGoForward(ChromeActivityTestRule<?> rule) throws Exception {
        ChromeActivity activity = rule.getActivity();
        String initialUrl = TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getCurrentWebContents().getLastCommittedUrl().getSpec());

        // Any built-in page that is not about:blank and is reasonably cheap to render will do,
        // here.
        Tab tab = activity.getTabModelSelector().getCurrentTab();
        String visitedUrl = "chrome://version/";
        assertThat(initialUrl, Matchers.not(Matchers.equalTo(visitedUrl)));
        rule.loadUrl(visitedUrl);
        ChromeTabUtils.waitForTabPageLoaded(tab, visitedUrl);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getCurrentWebContents().getNavigationController().goBack());
        ChromeTabUtils.waitForTabPageLoaded(tab, initialUrl);
        assertTrue(tab.canGoForward());
    }

    private DirectActionTestUtils() {
        // This is a utility class; it is not meant to be instantiated.
    }
}
