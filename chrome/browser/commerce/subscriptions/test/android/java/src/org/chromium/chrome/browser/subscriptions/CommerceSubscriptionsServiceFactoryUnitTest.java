// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.commerce.core.ShoppingService;

/** Unit tests for {@link CommerceSubscriptionsServiceFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CommerceSubscriptionsServiceFactoryUnitTest {

    @Rule public JniMocker mMocker = new JniMocker();

    @Mock private Profile mProfileOne;
    @Mock private Profile mIncognitoProfileOne;

    @Mock private Profile mProfileTwo;

    @Mock ShoppingService mShoppingService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(false).when(mProfileOne).isOffTheRecord();
        doReturn(mProfileOne).when(mProfileOne).getOriginalProfile();

        doReturn(true).when(mIncognitoProfileOne).isOffTheRecord();
        doReturn(mProfileOne).when(mIncognitoProfileOne).getOriginalProfile();

        doReturn(false).when(mProfileTwo).isOffTheRecord();
        doReturn(mProfileTwo).when(mProfileTwo).getOriginalProfile();
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
    }

    @Test
    @SmallTest
    public void testFactoryMethod() {
        CommerceSubscriptionsServiceFactory factory =
                CommerceSubscriptionsServiceFactory.getInstance();

        CommerceSubscriptionsService regularProfileOneService = factory.getForProfile(mProfileOne);
        Assert.assertEquals(regularProfileOneService, factory.getForProfile(mProfileOne));

        CommerceSubscriptionsService regularProfileTwoService = factory.getForProfile(mProfileTwo);
        Assert.assertNotEquals(regularProfileOneService, regularProfileTwoService);
        Assert.assertEquals(regularProfileTwoService, factory.getForProfile(mProfileTwo));

        Assert.assertEquals(regularProfileOneService, factory.getForProfile(mProfileOne));
        Assert.assertEquals(regularProfileTwoService, factory.getForProfile(mProfileTwo));

        Assert.assertEquals(regularProfileOneService, factory.getForProfile(mIncognitoProfileOne));
    }
}
