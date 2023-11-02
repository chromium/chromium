// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
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
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.content_public.browser.BrowserContextHandle;
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
    private CommerceSubscriptionsStorage.Natives mCommerceSubscriptionsStorageJni;

    @Mock
    EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    IdentityServicesProvider mIdentityServicesProvider;

    @Mock
    IdentityManager mIdentityManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(false).when(mProfileOne).isOffTheRecord();
        doReturn(false).when(mProfileTwo).isOffTheRecord();
        mMocker.mock(CommerceSubscriptionsStorageJni.TEST_HOOKS, mCommerceSubscriptionsStorageJni);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);

        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());

        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                CommerceSubscriptionsStorage storage =
                        (CommerceSubscriptionsStorage) invocation.getArguments()[0];
                storage.setNativeCommerceSubscriptionDBForTesting((long) 123);
                return null;
            }
        })
                .when(mCommerceSubscriptionsStorageJni)
                .init(any(CommerceSubscriptionsStorage.class), any(BrowserContextHandle.class));
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
