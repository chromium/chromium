// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.After;
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
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchGroup;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Unit tests for {@link AuxiliarySearchBridge}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION})
public final class AuxiliarySearchBridgeTest {
    private static final String TAB_TITLE = "tab";
    private static final String BOOKMAEK_TITLE = "bookmark";
    private static final String TAB_URL = "https://tab.google.com";
    private static final String BOOKMAEK_URL = "https://bookmark.google.com";
    private static final long FAKE_NATIVE_PROVIDER = 1;

    @Mock
    private AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    @Mock
    private Profile mProfile;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(AuxiliarySearchBridgeJni.TEST_HOOKS, mMockAuxiliarySearchBridgeJni);
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    public void getForProfileTest() {
        doReturn(false).when(mProfile).isOffTheRecord();
        AuxiliarySearchBridge bridge = new AuxiliarySearchBridge(mProfile);

        verify(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
    }

    @Test
    @SmallTest
    public void getSearchableDataTest() {
        doReturn(false).when(mProfile).isOffTheRecord();

        var tab =
                AuxiliarySearchGroup.Entry.newBuilder().setTitle(TAB_TITLE).setUrl(TAB_URL).build();
        var bookmark = AuxiliarySearchGroup.Entry.newBuilder()
                               .setTitle(BOOKMAEK_TITLE)
                               .setUrl(BOOKMAEK_URL)
                               .build();
        var proto = AuxiliarySearchGroup.newBuilder().addTabs(tab).addBookmarks(bookmark).build();

        doReturn(FAKE_NATIVE_PROVIDER).when(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
        doReturn(proto.toByteArray())
                .when(mMockAuxiliarySearchBridgeJni)
                .getSearchableData(FAKE_NATIVE_PROVIDER);

        AuxiliarySearchBridge bridge = new AuxiliarySearchBridge(mProfile);
        verify(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);

        AuxiliarySearchGroup group = bridge.getSearchableData();

        assertEquals(group.getTabsCount(), 1);
        assertEquals(group.getBookmarksCount(), 1);
        assertEquals(group.getTabs(0).getTitle(), TAB_TITLE);
        assertEquals(group.getTabs(0).getUrl(), TAB_URL);
        assertEquals(group.getBookmarks(0).getTitle(), BOOKMAEK_TITLE);
        assertEquals(group.getBookmarks(0).getUrl(), BOOKMAEK_URL);
    }
}
