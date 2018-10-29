// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.website.SiteDataCleaner;
import org.chromium.chrome.browser.preferences.website.Website;
import org.chromium.chrome.browser.preferences.website.WebsiteAddress;
import org.chromium.chrome.browser.preferences.website.WebsitePermissionsFetcher;
import org.chromium.chrome.browser.preferences.website.WebsitePermissionsFetcher.WebsitePermissionsCallback;
import org.chromium.chrome.browser.util.test.ShadowUrlUtilities;

import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/** Tests for {@link DomainDataCleaner}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowUrlUtilities.class})
public class DomainDataCleanerTest {
    private final Map<String, String> mUrlToDomain = new HashMap<>();
    @Mock
    private ChromeBrowserInitializer mChromeBrowserInitializer;
    @Mock
    private SiteDataCleaner mSiteDataCleaner;
    @Mock
    private WebsitePermissionsFetcher mWebsitePermissionsFetcher;
    @Mock
    private Runnable mFinishCallback;

    private DomainDataCleaner mCleaner;

    private Collection<Website> mSites;

    @Before
    public void setUp() {
        initMocks(this);

        doAnswer((Answer<Void>) invocation -> {
                    Runnable callback = invocation.getArgument(1);
                    callback.run();
                    return null;
        }).when(mSiteDataCleaner).clearData(any(), any());

        doAnswer((Answer<Void>) invocation -> {
            WebsitePermissionsCallback callback = invocation.getArgument(0);
            callback.onWebsitePermissionsAvailable(mSites);
            return null;
        }).when(mWebsitePermissionsFetcher).fetchAllPreferences(any());

        ShadowUrlUtilities.setTestImpl(new ShadowUrlUtilities.TestImpl() {
            @Override
            public String getDomainAndRegistry(String url, boolean includePrivateRegistries) {
                return mUrlToDomain.get(url);
            }
        });

        mCleaner = new DomainDataCleaner(
                mChromeBrowserInitializer, mSiteDataCleaner, mWebsitePermissionsFetcher);
    }

    @After
    public void tearDown() {
        ShadowUrlUtilities.reset();
    }

    @Test
    public void cleansDataOfSingleWebsite() {
        mSites = Arrays.asList(makeMockWebsite("http://www.facebook.com", "facebook.com"));
        mCleaner.clearData("facebook.com", mFinishCallback);
        verifyCleared("http://www.facebook.com");
        verify(mFinishCallback).run();
    }

    @Test
    public void cleansDataOfMultipleWebsites() {
        mSites = Arrays.asList(
                makeMockWebsite("http://www.facebook.com", "facebook.com"),
                makeMockWebsite("http://m.facebook.com", "facebook.com"),
                makeMockWebsite("http://p.facebook.com", "facebook.com"));
        mCleaner.clearData("facebook.com", mFinishCallback);
        verifyCleared("http://www.facebook.com");
        verifyCleared("http://m.facebook.com");
        verifyCleared("http://p.facebook.com");
        verify(mFinishCallback).run();
    }

    @Test
    public void doesntCleanDataOfIrrelevantWebsites() {
        mSites = Arrays.asList(
                makeMockWebsite("http://www.google.com", "google.com"),
                makeMockWebsite("http://www.twitter.com", "twitter.com"));
        mCleaner.clearData("facebook.com", mFinishCallback);
        verify(mSiteDataCleaner, never()).clearData(any(), any());
        verify(mFinishCallback).run();
    }

    private Website makeMockWebsite(String origin, String domain) {
        Website website = new Website(WebsiteAddress.create(origin), null);
        mUrlToDomain.put(origin, domain);
        return website;
    }

    private void verifyCleared(String origin) {
        verify(mSiteDataCleaner)
                .clearData(argThat(argument -> argument.getAddress().getOrigin().equals(origin)),
                        any());
    }
}
