// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.not;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Locale;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests that verify the end to end behavior of appending referral IDs to search engines.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class LocaleManagerReferralTest {
    private Locale mDefaultLocale;
    private String mYandexReferralId = "";

    @Before
    public void setUp() throws ExecutionException {
        mDefaultLocale = Locale.getDefault();
        Locale.setDefault(new Locale("ru", "RU"));

        LocaleManager.setInstanceForTest(new LocaleManager() {
            @Override
            protected String getYandexReferralId() {
                return mYandexReferralId;
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(new Callable<Void>() {
            @Override
            public Void call() {
                ChromeBrowserInitializer.getInstance(InstrumentationRegistry.getTargetContext())
                        .handleSynchronousStartup();
                return null;
            }
        });
    }

    @After
    public void tearDown() {
        Locale.setDefault(mDefaultLocale);
    }

    @SmallTest
    @Test
    public void testYandexReferralId() throws TimeoutException {
        final CallbackHelper templateUrlServiceLoaded = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
            templateUrlService.registerLoadListener(new TemplateUrlService.LoadListener() {
                @Override
                public void onTemplateUrlServiceLoaded() {
                    templateUrlServiceLoaded.notifyCalled();
                }
            });

            templateUrlService.load();
        });

        templateUrlServiceLoaded.waitForCallback("Template URLs never loaded", 0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TemplateUrlServiceFactory.get().setSearchEngine("yandex.ru");

            // The initial param is empty, so ensure no clid param is passed.
            String url = TemplateUrlServiceFactory.get().getUrlForSearchQuery("blah");
            Assert.assertThat(url, not(containsString("&clid=")));

            // Initialize the value to something and verify it is included in the generated
            // URL.
            mYandexReferralId = "TESTING_IS_AWESOME";
            url = TemplateUrlServiceFactory.get().getUrlForSearchQuery("blah");
            Assert.assertThat(url, containsString("&clid=TESTING_IS_AWESOME"));

            // Switch to google and ensure the clid param is no longer included.
            TemplateUrlServiceFactory.get().setSearchEngine("google.com");
            url = TemplateUrlServiceFactory.get().getUrlForSearchQuery("blah");
            Assert.assertThat(url, not(containsString("&clid=")));
        });
    }
}
