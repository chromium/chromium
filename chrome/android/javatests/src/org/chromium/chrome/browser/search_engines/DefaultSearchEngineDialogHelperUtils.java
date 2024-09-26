// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.view.View;
import android.view.ViewGroup;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.R;

/** Utilities for interacting with a {@link DefaultSearchEngineDialogHelper}. */
public class DefaultSearchEngineDialogHelperUtils {
    private static final int OPTION_LAYOUT_ID = R.id.default_search_engine_dialog_options;
    private static final int OK_BUTTON_ID = R.id.button_primary;

    private static String sSelectedEngine;

    /** Clicks on the first search engine option available. */
    public static void clickOnFirstEngine(final View rootView) {
        // Wait for the options to appear.
        CriteriaHelper.pollUiThread(
                () -> {
                    ViewGroup options = rootView.findViewById(OPTION_LAYOUT_ID);
                    Criteria.checkThat(options.getChildCount(), Matchers.greaterThan(0));
                });

        // Click on the first search engine option available.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup options = rootView.findViewById(OPTION_LAYOUT_ID);
                    options.getChildAt(0).performClick();
                    sSelectedEngine = (String) options.getChildAt(0).getTag();
                });

        // Wait for the OK button to be clicakble.
        CriteriaHelper.pollUiThread(
                () -> {
                    View view = rootView.findViewById(OK_BUTTON_ID);
                    Criteria.checkThat(view, Matchers.notNullValue());
                    Criteria.checkThat(view.isEnabled(), Matchers.is(true));
                });

        // Click on the OK button.
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    View view = rootView.findViewById(OK_BUTTON_ID);
                    view.performClick();
                });

        // Confirm the engine was set appropriately.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertEquals(
                                "Search engine wasn't set",
                                TemplateUrlServiceFactory.getForProfile(
                                                ProfileManager.getLastUsedRegularProfile())
                                        .getDefaultSearchEngineTemplateUrl()
                                        .getKeyword(),
                                sSelectedEngine));
    }
}
