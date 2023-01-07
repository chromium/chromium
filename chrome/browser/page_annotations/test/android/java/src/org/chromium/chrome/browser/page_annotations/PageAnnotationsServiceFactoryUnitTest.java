// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_annotations;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Unit tests for {@link PageAnnotationsServiceFactory}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PageAnnotationsServiceFactoryUnitTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock
    private Profile mProfileOne;

    @Mock
    private Profile mProfileTwo;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testFactoryMethod() {
        PageAnnotationsServiceFactory factory = new PageAnnotationsServiceFactory();
        PageAnnotationsService regularProfileService = factory.getForLastUsedProfile();
        Assert.assertEquals(regularProfileService, factory.getForLastUsedProfile());

        Profile.setLastUsedProfileForTesting(mProfileOne);
        PageAnnotationsService regularProfileOneService = factory.getForLastUsedProfile();
        Assert.assertNotEquals(regularProfileService, regularProfileOneService);
        Assert.assertEquals(regularProfileOneService, factory.getForLastUsedProfile());

        Profile.setLastUsedProfileForTesting(mProfileTwo);
        PageAnnotationsService regularProfileTwoService = factory.getForLastUsedProfile();
        Assert.assertNotEquals(regularProfileService, regularProfileTwoService);
        Assert.assertEquals(regularProfileTwoService, factory.getForLastUsedProfile());

        Profile.setLastUsedProfileForTesting(null);
        Assert.assertEquals(regularProfileService, factory.getForLastUsedProfile());

        Profile.setLastUsedProfileForTesting(mProfileOne);
        Assert.assertEquals(regularProfileOneService, factory.getForLastUsedProfile());

        Profile.setLastUsedProfileForTesting(mProfileTwo);
        Assert.assertEquals(regularProfileTwoService, factory.getForLastUsedProfile());
    }
}
