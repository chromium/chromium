// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.commerce.core.ShoppingService;

/**
 * Unit tests for {@link CommerceSubscriptionsServiceFactory}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CommerceSubscriptionsServiceFactoryUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Mock
    private Profile mProfileOne;

    @Mock
    private Profile mProfileTwo;

    @Mock
    EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    ShoppingService mShoppingService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(false).when(mProfileOne).isOffTheRecord();
        doReturn(false).when(mProfileTwo).isOffTheRecord();
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
    }

    @After
    public void tearDown() {
        IdentityServicesProvider.setInstanceForTests(null);
    }

    @Test
    @SmallTest
    public void testFactoryMethod() {
        CommerceSubscriptionsServiceFactory factory = new CommerceSubscriptionsServiceFactory();

        Profile.setLastUsedProfileForTesting(mProfileOne);
        CommerceSubscriptionsService regularProfileOneService = factory.getForLastUsedProfile();
        Assert.assertEquals(regularProfileOneService, factory.getForLastUsedProfile());

        Profile.setLastUsedProfileForTesting(mProfileTwo);
        CommerceSubscriptionsService regularProfileTwoService = factory.getForLastUsedProfile();
        Assert.assertNotEquals(regularProfileOneService, regularProfileTwoService);
        Assert.assertEquals(regularProfileTwoService, factory.getForLastUsedProfile());

        Profile.setLastUsedProfileForTesting(mProfileOne);
        Assert.assertEquals(regularProfileOneService, factory.getForLastUsedProfile());

        Profile.setLastUsedProfileForTesting(mProfileTwo);
        Assert.assertEquals(regularProfileTwoService, factory.getForLastUsedProfile());
    }

    @Test
    @SmallTest
    public void testServiceDestroyedWhenProfileIsDestroyed() {
        CommerceSubscriptionsServiceFactory factory = new CommerceSubscriptionsServiceFactory();
        Profile.setLastUsedProfileForTesting(mProfileOne);
        CommerceSubscriptionsService service = factory.getForLastUsedProfile();
        Assert.assertEquals(
                1, CommerceSubscriptionsServiceFactory.sProfileToSubscriptionsService.size());
        ProfileManager.onProfileDestroyed(mProfileOne);
        Assert.assertTrue(
                CommerceSubscriptionsServiceFactory.sProfileToSubscriptionsService.isEmpty());
    }
}
