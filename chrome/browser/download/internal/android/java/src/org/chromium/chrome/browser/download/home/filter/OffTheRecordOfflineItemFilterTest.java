// Copyright 2018 The Chromium Authors. All rights reserved.
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
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.OTRProfileIDJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;

/** Unit tests for the TypeOfflineItemFilter class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OffTheRecordOfflineItemFilterTest {
    public static final String PRIMARY_OTR_PROFILE_ID = "profile::primary_otr";

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Mock
    private OTRProfileID.Natives mOTRProfileIDNatives;

    @Mock
    private OfflineItemFilterSource mSource;

    @Mock
    private OfflineItemFilterObserver mObserver;

    @Mock
    private Profile mRegularProfile;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setup() {
        Profile.setLastUsedProfileForTesting(mRegularProfile);
        mMocker.mock(OTRProfileIDJni.TEST_HOOKS, mOTRProfileIDNatives);
        when(mRegularProfile.hasOffTheRecordProfile(any())).thenReturn(true);
        when(OTRProfileIDJni.get().getPrimaryID())
                .thenReturn(new OTRProfileID(PRIMARY_OTR_PROFILE_ID));
    }

    @Test
    public void testPassthrough() {
        OfflineItem item1 = buildItem(true);
        OfflineItem item2 = buildItem(false);
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2);
        when(mSource.getItems()).thenReturn(sourceItems);

        OffTheRecordOfflineItemFilter filter = new OffTheRecordOfflineItemFilter(true, mSource);
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item2), filter.getItems());
    }

    @Test
    public void testFiltersOutItems() {
        OfflineItem item1 = buildItem(true);
        OfflineItem item2 = buildItem(false);
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2);
        when(mSource.getItems()).thenReturn(sourceItems);

        OffTheRecordOfflineItemFilter filter = new OffTheRecordOfflineItemFilter(false, mSource);
        Assert.assertEquals(CollectionUtil.newHashSet(item2), filter.getItems());
    }

    @Test
    public void testFiltersOutItemsForNonPrimaryOTRProfiles() {
        OfflineItem item1 = buildItem(true);
        OfflineItem item2 = buildItem(false);
        OfflineItem item3 = buildItemWithOTRProfileId(true, "profile::CCT-Test");
        Collection<OfflineItem> sourceItems = CollectionUtil.newHashSet(item1, item2, item3);
        when(mSource.getItems()).thenReturn(sourceItems);

        OffTheRecordOfflineItemFilter filter = new OffTheRecordOfflineItemFilter(true, mSource);
        Assert.assertEquals(CollectionUtil.newHashSet(item1, item2), filter.getItems());
    }

    private static OfflineItem buildItem(boolean isOffTheRecord) {
        return buildItemWithOTRProfileId(isOffTheRecord, null);
    }

    private static OfflineItem buildItemWithOTRProfileId(
            boolean isOffTheRecord, String otrProfileID) {
        OfflineItem item = new OfflineItem();
        item.isOffTheRecord = isOffTheRecord;
        if (isOffTheRecord) {
            if (otrProfileID == null) otrProfileID = PRIMARY_OTR_PROFILE_ID;
            item.otrProfileId = OTRProfileID.serialize(new OTRProfileID(otrProfileID));
        }
        return item;
    }
}