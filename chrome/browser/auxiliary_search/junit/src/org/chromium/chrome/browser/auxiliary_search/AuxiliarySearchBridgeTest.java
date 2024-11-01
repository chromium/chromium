// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchBookmarkGroup;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link AuxiliarySearchBridge} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION})
public final class AuxiliarySearchBridgeTest {
    private static final String BOOKMARK_TITLE = "bookmark";
    private static final String BOOKMARK_URL = "https://bookmark.google.com";
    private static final String TAB_TITLE = "tab";
    private static final String TAB_URL = "https://tab.google.com";
    private static final long FAKE_NATIVE_PROVIDER = 1;

    @Mock private AuxiliarySearchBridge.Natives mMockAuxiliarySearchBridgeJni;
    @Mock private Profile mProfile;

    @Rule public JniMocker mJniMocker = new JniMocker();

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
        assertNotNull(bridge);

        verify(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
    }

    @Test
    @SmallTest
    public void getBookmarksSearchableData() {
        doReturn(false).when(mProfile).isOffTheRecord();

        var bookmark =
                AuxiliarySearchEntry.newBuilder()
                        .setTitle(BOOKMARK_TITLE)
                        .setUrl(BOOKMARK_URL)
                        .build();
        var proto = AuxiliarySearchBookmarkGroup.newBuilder().addBookmark(bookmark).build();

        doReturn(FAKE_NATIVE_PROVIDER).when(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);
        doReturn(proto.toByteArray())
                .when(mMockAuxiliarySearchBridgeJni)
                .getBookmarksSearchableData(FAKE_NATIVE_PROVIDER);

        AuxiliarySearchBridge bridge = new AuxiliarySearchBridge(mProfile);
        assertNotNull(bridge);
        verify(mMockAuxiliarySearchBridgeJni).getForProfile(mProfile);

        AuxiliarySearchBookmarkGroup group = bridge.getBookmarksSearchableData();

        assertEquals(group.getBookmarkCount(), 1);
        assertEquals(group.getBookmark(0).getTitle(), BOOKMARK_TITLE);
        assertEquals(group.getBookmark(0).getUrl(), BOOKMARK_URL);
    }
}
