// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.content_settings.ContentSettingsType;

/** Tests for various Safety Hub settings surfaces. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class SafetyHubTest {
    private static final PermissionsData PERMISSIONS_DATA =
            PermissionsData.create(
                    "http://example.com",
                    new int[] {
                        ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingsType.MEDIASTREAM_MIC
                    },
                    0,
                    0);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public SettingsActivityTestRule<SafetyHubPermissionsFragment> mPermissionsFragmentTestRule =
            new SettingsActivityTestRule<>(SafetyHubPermissionsFragment.class);

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock public UnusedSitePermissionsBridge.Natives mUnusedPermissionsBridge;

    @Before
    public void setUp() {
        mJniMocker.mock(UnusedSitePermissionsBridgeJni.TEST_HOOKS, mUnusedPermissionsBridge);
    }

    @Test
    @SmallTest
    public void testPermissionRegrant() {
        when(mUnusedPermissionsBridge.getRevokedPermissions(any()))
                .thenReturn(new PermissionsData[] {PERMISSIONS_DATA});
        mPermissionsFragmentTestRule.startSettingsActivity();

        clickOnButtonNextToText(PERMISSIONS_DATA.getOrigin());
        onView(withText(PERMISSIONS_DATA.getOrigin())).check(doesNotExist());
    }

    private void clickOnButtonNextToText(String text) {
        onView(allOf(withId(R.id.button), withParent(hasSibling(withChild(withText(text))))))
                .perform(click());
    }
}
