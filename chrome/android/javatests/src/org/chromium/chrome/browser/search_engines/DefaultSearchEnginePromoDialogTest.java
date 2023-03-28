// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.app.Activity;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/**
 * Integration tests for the {@link DefaultSearchEnginePromoDialog}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DefaultSearchEnginePromoDialogTest {
    @Before
    public void setUp() throws ExecutionException {
        TestThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
            @Override
            public Void call() {
                ChromeBrowserInitializer.getInstance().handleSynchronousStartup();

                LocaleManagerDelegate mockDelegate = new LocaleManagerDelegate() {
                    @Override
                    public List<TemplateUrl> getSearchEnginesForPromoDialog(int promoType) {
                        return getTemplateUrlService().getTemplateUrls();
                    }
                };
                LocaleManager.getInstance().setDelegateForTest(mockDelegate);
                return null;
            }
        });
    }

    private TemplateUrlService getTemplateUrlService() {
        return TemplateUrlServiceFactory.getForProfile(Profile.getLastUsedRegularProfile());
    }

    @Test
    @LargeTest
    public void testOnlyOneLiveDialog() throws Exception {
        final CallbackHelper templateUrlServiceInit = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> getTemplateUrlService().registerLoadListener(
                                new TemplateUrlService.LoadListener() {
                                    @Override
                                    public void onTemplateUrlServiceLoaded() {
                                        getTemplateUrlService().unregisterLoadListener(this);
                                        templateUrlServiceInit.notifyCalled();
                                    }
                                }));
        templateUrlServiceInit.waitForCallback(0);

        final SearchActivity searchActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SearchActivity.class);
        final DefaultSearchEnginePromoDialog searchDialog = showDialog(searchActivity);
        Assert.assertEquals(searchDialog, DefaultSearchEnginePromoDialog.getCurrentDialog());

        ChromeTabbedActivity tabbedActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class);
        final DefaultSearchEnginePromoDialog tabbedDialog = showDialog(tabbedActivity);
        Assert.assertEquals(tabbedDialog, DefaultSearchEnginePromoDialog.getCurrentDialog());

        CriteriaHelper.pollUiThread(() -> !searchDialog.isShowing());
        CriteriaHelper.pollUiThread(() -> searchActivity.isFinishing());

        TestThreadUtils.runOnUiThreadBlocking(() -> tabbedDialog.dismiss());
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    DefaultSearchEnginePromoDialog.getCurrentDialog(), Matchers.nullValue());
        });
    }

    private DefaultSearchEnginePromoDialog showDialog(final Activity activity)
            throws ExecutionException {
        DefaultSearchEngineDialogHelper.Delegate delegate =
                new DefaultSearchEngineDialogHelper.Delegate() {
                    @Override
                    public List<TemplateUrl> getSearchEnginesForPromoDialog(
                            @SearchEnginePromoType int type) {
                        return new ArrayList<>();
                    }

                    @Override
                    public void onUserSearchEngineChoice(@SearchEnginePromoType int type,
                            List<String> keywords, String keyword) {}
                };
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            DefaultSearchEnginePromoDialog dialog = new DefaultSearchEnginePromoDialog(
                    activity, delegate, SearchEnginePromoType.SHOW_EXISTING, null);
            dialog.show();
            return dialog;
        });
    }
}
