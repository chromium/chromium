// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit tests for {@link AppMenuItemTheme}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AppMenuItemThemeUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Profile mProfile;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsJniMock;

    private AppMenuItemTheme mAppMenuItemTheme;

    @Before
    public void setUp() {
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsJniMock);
        mAppMenuItemTheme = new AppMenuItemTheme(mContext, mTabModelSelector);
    }

    @Test
    public void isMenuItemManaged_IncognitoAvailableByPolicy() {
        when(mIncognitoUtilsJniMock.getIncognitoModeManaged(mProfile)).thenReturn(true);
        when(mIncognitoUtilsJniMock.getIncognitoModeEnabled(mProfile)).thenReturn(true);

        assertFalse(mAppMenuItemTheme.isMenuItemManaged(R.id.new_incognito_tab_menu_id));
    }

    @Test
    public void isMenuItemManaged_IncognitoDisabledByPolicy() {
        when(mIncognitoUtilsJniMock.getIncognitoModeManaged(mProfile)).thenReturn(true);
        when(mIncognitoUtilsJniMock.getIncognitoModeEnabled(mProfile)).thenReturn(false);

        assertTrue(mAppMenuItemTheme.isMenuItemManaged(R.id.new_incognito_tab_menu_id));
    }
}
