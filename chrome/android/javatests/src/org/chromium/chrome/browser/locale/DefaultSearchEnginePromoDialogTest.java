// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

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
                ChromeBrowserInitializer.getInstance(InstrumentationRegistry.getTargetContext())
                        .handleSynchronousStartup();

                LocaleManager mockManager = new LocaleManager() {
                    @Override
                    public List<TemplateUrl> getSearchEnginesForPromoDialog(int promoType) {
                        return TemplateUrlServiceFactory.get().getTemplateUrls();
                    }
                };
                LocaleManager.setInstanceForTest(mockManager);
                return null;
            }
        });
    }

    @Test
    @LargeTest
    public void testOnlyOneLiveDialog() throws Exception {
        final CallbackHelper templateUrlServiceInit = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TemplateUrlServiceFactory.get().registerLoadListener(
                                new TemplateUrlService.LoadListener() {
                                    @Override
                                    public void onTemplateUrlServiceLoaded() {
                                        TemplateUrlServiceFactory.get().unregisterLoadListener(
                                                this);
                                        templateUrlServiceInit.notifyCalled();
                                    }
                                }));
        templateUrlServiceInit.waitForCallback(0);

        final SearchActivity searchActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SearchActivity.class);
        final DefaultSearchEnginePromoDialog searchDialog = showDialog(searchActivity);
        Assert.assertEquals(searchDialog, DefaultSearchEnginePromoDialog.getCurrentDialog());

        ChromeTabbedActivity tabbedActivity = ActivityUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), ChromeTabbedActivity.class);
        final DefaultSearchEnginePromoDialog tabbedDialog = showDialog(tabbedActivity);
        Assert.assertEquals(tabbedDialog, DefaultSearchEnginePromoDialog.getCurrentDialog());

        CriteriaHelper.pollUiThread(Criteria.equals(false, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return searchDialog.isShowing();
            }
        }));

        CriteriaHelper.pollUiThread(Criteria.equals(true, new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return searchActivity.isFinishing();
            }
        }));

        TestThreadUtils.runOnUiThreadBlocking(() -> tabbedDialog.dismiss());
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return DefaultSearchEnginePromoDialog.getCurrentDialog() == null;
            }
        });
    }

    private DefaultSearchEnginePromoDialog showDialog(final Activity activity)
            throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                new Callable<DefaultSearchEnginePromoDialog>() {
                    @Override
                    public DefaultSearchEnginePromoDialog call() {
                        DefaultSearchEnginePromoDialog dialog = new DefaultSearchEnginePromoDialog(
                                activity, LocaleManager.SearchEnginePromoType.SHOW_EXISTING, null);
                        dialog.show();
                        return dialog;
                    }
                });
    }
}
