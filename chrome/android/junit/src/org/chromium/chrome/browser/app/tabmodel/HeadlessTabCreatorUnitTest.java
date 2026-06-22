// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertThrows;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;

/** Unit tests for {@link HeadlessTabCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HeadlessTabCreatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;

    private HeadlessTabCreator mDisabledCreator;

    @Before
    public void setUp() {
        mDisabledCreator = new HeadlessTabCreator(mProfile, /* isIncognito= */ true);
    }

    @Test
    public void testDisabledCreatorThrows() {
        LoadUrlParams loadUrlParams = new LoadUrlParams("https://example.com");
        assertThrows(
                UnsupportedOperationException.class,
                () ->
                        mDisabledCreator.createNewTab(
                                loadUrlParams,
                                TabLaunchType.FROM_LINK,
                                null));
    }
}
