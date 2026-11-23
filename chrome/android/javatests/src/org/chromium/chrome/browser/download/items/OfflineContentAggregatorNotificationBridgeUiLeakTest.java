// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.items;

import android.graphics.Bitmap;

import androidx.test.filters.LargeTest;

import leakcanary.AppWatcher;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.util.List;
import java.util.UUID;

/** Leak instrumentation tests for {@link OfflineContentAggregatorNotificationBridgeUi}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
@NullMarked
public class OfflineContentAggregatorNotificationBridgeUiLeakTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Test
    @LargeTest
    public void offlineItemVisualsReleasedOnCompletionTest() {
        mActivityTestRule.startOnBlankPage();
        completeItemWithWatchedVisuals();
    }

    private static void completeItemWithWatchedVisuals() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    OfflineContentAggregatorNotificationBridgeUi bridge =
                            OfflineContentAggregatorNotificationBridgeUiFactory.instance();
                    ContentId id =
                            new ContentId("offline_visuals_check", UUID.randomUUID().toString());

                    OfflineItem inProgress = new OfflineItem();
                    inProgress.id = id;
                    inProgress.state = OfflineItemState.IN_PROGRESS;
                    inProgress.progress = OfflineItem.Progress.createIndeterminateProgress();
                    inProgress.title = "sample.png";
                    inProgress.filePath = "/sdcard/Download/sample.png";
                    inProgress.isOpenable = false;
                    inProgress.isTransient = true;
                    bridge.onItemsAdded(List.of(inProgress));

                    OfflineItemVisuals visuals = new OfflineItemVisuals();
                    visuals.icon = Bitmap.createBitmap(64, 64, Bitmap.Config.ARGB_8888);
                    AppWatcher.INSTANCE
                            .getObjectWatcher()
                            .expectWeaklyReachable(
                                    visuals, "OfflineItemVisuals for completed OfflineItem");
                    bridge.onVisualsAvailable(id, visuals);

                    OfflineItem completed = new OfflineItem();
                    completed.id = id;
                    completed.state = OfflineItemState.COMPLETE;
                    completed.title = "sample.png";
                    completed.filePath = "/sdcard/Download/sample.png";
                    completed.isOpenable = false;
                    completed.isTransient = true;
                    UpdateDelta delta = new UpdateDelta();
                    delta.stateChanged = true;
                    bridge.onItemUpdated(completed, delta);

                    DownloadManagerService.getDownloadManagerService()
                            .getDownloadNotifier()
                            .notifyDownloadCanceled(id);
                });
    }
}
