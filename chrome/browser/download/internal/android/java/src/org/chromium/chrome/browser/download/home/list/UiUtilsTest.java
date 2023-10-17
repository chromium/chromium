// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

/** Unit tests for the UiUtils class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UiUtilsTest {
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        mShadowPackageManager = Shadows.shadowOf(context.getPackageManager());
    }

    @Test
    public void testCanShare_completeLegacyDownload_returnsTrue() {
        assertTrue(
                UiUtils.canShare(
                        createOfflineItem(
                                new ContentId("LEGACY_DOWNLOAD", "A"), OfflineItemState.COMPLETE)));
    }

    @Test
    public void testCanShare_completeLegacyOfflinePage_returnsTrue() {
        assertTrue(
                UiUtils.canShare(
                        createOfflineItem(
                                new ContentId("LEGACY_OFFLINE_PAGE", "A"),
                                OfflineItemState.COMPLETE)));
    }

    @Test
    public void testCanShare_pendingDownload_returnsFalse() {
        assertFalse(
                UiUtils.canShare(
                        createOfflineItem(
                                new ContentId("LEGACY_DOWNLOAD", "A"), OfflineItemState.PENDING)));
    }

    @Test
    public void testCanShare_nonLegacyDownload_returnsFalse() {
        assertFalse(
                UiUtils.canShare(
                        createOfflineItem(new ContentId("test", "A"), OfflineItemState.COMPLETE)));
    }

    @Test
    public void testCanShare_isAutomotive_returnsFalse() {
        mShadowPackageManager.setSystemFeature(
                PackageManager.FEATURE_AUTOMOTIVE, /* supported= */ true);
        assertFalse(
                UiUtils.canShare(
                        createOfflineItem(
                                new ContentId("LEGACY_DOWNLOAD", "A"), OfflineItemState.COMPLETE)));
    }

    private static OfflineItem createOfflineItem(ContentId id, @OfflineItemState int state) {
        OfflineItem item = new OfflineItem();
        item.id = id;
        item.state = state;
        return item;
    }
}
