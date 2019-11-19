// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Utilities for interacting with a {@link DefaultSearchEngineDialogHelper}.
 */
public class DefaultSearchEngineDialogHelperUtils {
    private static final int OPTION_LAYOUT_ID = R.id.default_search_engine_dialog_options;
    private static final int OK_BUTTON_ID = R.id.button_primary;

    private static String sSelectedEngine;

    /** Clicks on the first search engine option available. */
    public static void clickOnFirstEngine(final View rootView) {
        // Wait for the options to appear.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                ViewGroup options = (ViewGroup) rootView.findViewById(OPTION_LAYOUT_ID);
                return options.getChildCount() > 0;
            }
        });

        // Click on the first search engine option available.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup options = (ViewGroup) rootView.findViewById(OPTION_LAYOUT_ID);
            options.getChildAt(0).performClick();
            sSelectedEngine = (String) (options.getChildAt(0).getTag());
        });

        // Wait for the OK button to be clicakble.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                View view = rootView.findViewById(OK_BUTTON_ID);
                return view != null && view.isEnabled();
            }
        });

        // Click on the OK button.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            View view = rootView.findViewById(OK_BUTTON_ID);
            view.performClick();
        });

        // Confirm the engine was set appropriately.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertEquals("Search engine wasn't set",
                                TemplateUrlServiceFactory.get()
                                        .getDefaultSearchEngineTemplateUrl()
                                        .getKeyword(),
                                sSelectedEngine));
    }
}
