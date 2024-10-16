// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.CollectionUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.OtrProfileIdJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;

/** Unit tests for the TypeOfflineItemFilter class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OffTheRecordOfflineItemFilterTest {
    @Rule public JniMocker mMocker = new JniMocker();

    @Mock private OtrProfileId.Natives mOtrProfileIdNatives;

    @Mock private OfflineItemFilterSource mSource;

    @Mock private OfflineItemFilterObserver mObserver;

    @Mock private Profile mRegularProfile;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setup() {
        ProfileManager.setLastUsedProfileForTesting(mRegularProfile);
        mMocker.mock(OtrProfileIdJni.TEST_HOOKS, mOtrProfileIdNatives);
        when(mRegularProfile.hasOffTheRecordProfile(any())).thenReturn(true);
    }

    @Test
    public void testPassthrough() {
        OfflineItem item1 = buildItem(OtrProfileId.getPrimaryOtrProfileId());
        OfflineItem item2 = buildItem(null);
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2);
        when(mSource.getItems()).thenReturn(sourceItems);

        OffTheRecordOfflineItemFilter filter = new OffTheRecordOfflineItemFilter(true, mSource);
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item2), filter.getItems());
    }

    @Test
    public void testFiltersOutItems() {
        OfflineItem item1 = buildItem(OtrProfileId.getPrimaryOtrProfileId());
        OfflineItem item2 = buildItem(null);
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2);
        when(mSource.getItems()).thenReturn(sourceItems);

        OffTheRecordOfflineItemFilter filter = new OffTheRecordOfflineItemFilter(false, mSource);
        Assert.assertEquals(CollectionUtil.newHashSet(item2), filter.getItems());
    }

    @Test
    public void testFiltersOutItemsForNonPrimaryOtrProfiles() {
        OfflineItem item1 = buildItem(OtrProfileId.getPrimaryOtrProfileId());
        OfflineItem item2 = buildItem(null);
        OfflineItem item3 = buildItem(new OtrProfileId("profile::CCT-Test"));
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2, item3);
        when(mSource.getItems()).thenReturn(sourceItems);

        OffTheRecordOfflineItemFilter filter = new OffTheRecordOfflineItemFilter(true, mSource);
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item2), filter.getItems());
    }

    private static OfflineItem buildItem(OtrProfileId otrProfileId) {
        OfflineItem item = new OfflineItem();
        item.isOffTheRecord = OtrProfileId.isOffTheRecord(otrProfileId);
        item.otrProfileId = OtrProfileId.serialize(otrProfileId);
        return item;
    }
}
