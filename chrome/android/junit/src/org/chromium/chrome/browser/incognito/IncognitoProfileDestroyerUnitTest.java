// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerJni;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit tests for {@link IncognitoProfileDestroyer}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class IncognitoProfileDestroyerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ProfileManager.Natives mProfileManagerJni;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private Profile mOtrProfileA;
    @Mock private IncognitoTabHost mOtherHost;

    @Before
    public void setUp() {
        ProfileManagerJni.setInstanceForTesting(mProfileManagerJni);
        IncognitoTabHostRegistry.getInstance().clearForTesting();
        when(mTabModelSelector.getModel(/* incognito= */ true)).thenReturn(mIncognitoTabModel);
        when(mIncognitoTabModel.getProfile()).thenReturn(mOtrProfileA);
        when(mOtrProfileA.isOffTheRecord()).thenReturn(true);
    }

    @After
    public void tearDown() {
        IncognitoTabHostRegistry.getInstance().clearForTesting();
    }

    @Test
    public void testDidBecomeEmpty_destroysProfileWhenOtherHostOnlyActiveForDifferentProfile() {
        // Other host has incognito tabs for another profile (globally non-empty), but not for
        // mOtrProfileA.
        when(mOtherHost.hasIncognitoTabs(/* otrProfile= */ null)).thenReturn(true);
        IncognitoTabHostRegistry.getInstance().register(mOtherHost);

        new IncognitoProfileDestroyer(mTabModelSelector).didBecomeEmpty();

        verify(mProfileManagerJni).destroyWhenAppropriate(mOtrProfileA);
    }

    @Test
    public void
            testDidBecomeEmpty_doesNotDestroyProfileWhenOtherHostHasTabsOrActiveModelForSameProfile() {
        IncognitoTabHostRegistry.getInstance().register(mOtherHost);

        when(mOtherHost.hasIncognitoTabs(mOtrProfileA)).thenReturn(true);
        IncognitoProfileDestroyer destroyer = new IncognitoProfileDestroyer(mTabModelSelector);
        destroyer.didBecomeEmpty();
        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());

        when(mOtherHost.hasIncognitoTabs(mOtrProfileA)).thenReturn(false);
        when(mOtherHost.isActiveModel(mOtrProfileA)).thenReturn(true);
        destroyer.didBecomeEmpty();
        verify(mProfileManagerJni, never()).destroyWhenAppropriate(any());
    }
}
