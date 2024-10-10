// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.LongScreenshotsEntry.EntryStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.paintpreview.player.CompositorStatus;

import java.util.ArrayList;
import java.util.List;

/**
 * Entry manager responsible for managing all the of the {@LongScreenshotEntry}. This should be used
 * to generate and retrieve the needed bitmaps. Currently we generate the screenshot in one pass;
 * to obtain it call {@link generateFullpageEntry}.
 */
public class EntryManager {
    private static final int KB_IN_BYTES = 1024;
    // List of all entries in correspondence of the webpage.
    private List<LongScreenshotsEntry> mEntries;
    // List of entries that are queued to generate the bitmap. Entries should only be queued
    // while the capture is in progress.
    private List<LongScreenshotsEntry> mQueuedEntries;
    private BitmapGenerator mGenerator;
    private ObserverList<BitmapGeneratorObserver> mGeneratorObservers;
    private @EntryStatus int mGeneratorStatus;
    private ScreenshotBoundsManager mBoundsManager;

    /**
     * Users of the {@link EntryManager} can implement this interface to be notified of changes to
     * the generator.
     */
    public interface BitmapGeneratorObserver {
        /**
         * Called when the generator status changes.
         * @param status current status.
         */
        void onStatusChange(@EntryStatus int status);

        /**
         * Called when the compositor is ready.
         * @param contentSize size of the main frame.
         * @param scrollOffset the offset of the viewport rect relative to the main frame.
         */
        void onCompositorReady(Size contentSize, Point scrollOffset);
    }

    /**
     * @param context An instance of current Android {@link Context}.
     * @param tab Tab to generate the bitmap for.
     * @param inMemory Use memory buffers to store the capture rather than temporary files.
     */
    public EntryManager(Context context, Tab tab, boolean inMemory) {
        this(new ScreenshotBoundsManager(context, tab), tab, inMemory);
    }

    /**
     * @param boundsManager A {@link ScreenshotBoundsManager}.
     * @param tab Tab to generate the bitmap for.
     * @param inMemory Use memory buffers to store the capture rather than temporary files.
     */
    public EntryManager(ScreenshotBoundsManager boundsManager, Tab tab, boolean inMemory) {
        mEntries = new ArrayList<LongScreenshotsEntry>();
        mQueuedEntries = new ArrayList<LongScreenshotsEntry>();
        mGeneratorObservers = new ObserverList<>();
        mBoundsManager = boundsManager;

        mGenerator = new BitmapGenerator(tab, mBoundsManager, createBitmapGeneratorCallback());
        mGenerator.captureTab(inMemory);
        updateGeneratorStatus(EntryStatus.CAPTURE_IN_PROGRESS);
    }

    public BitmapGenerator getBitmapGeneratorForTesting() {
        return mGenerator;
    }

    /**
     * Generates the long screenshot for the editor window. Callers of this function should add a
     * listener to the returned entry to get that status of the generation and retrieve the bitmap.
     */
    public LongScreenshotsEntry generateFullpageEntry() {
        LongScreenshotsEntry entry =
                new LongScreenshotsEntry(mGenerator, mBoundsManager.getFullEntryBounds(), null);
        processEntry(entry, false, false);
        return entry;
    }

    /**
     * Generates the bitmap of content within the bounds passed.
     *
     * @param bounds bounds to generate the bitmap from.
     * @return The new entry that generates the bitmap.
     */
    public LongScreenshotsEntry generateEntry(Rect bounds) {
        LongScreenshotsEntry entry = new LongScreenshotsEntry(mGenerator, bounds, (bytes) -> {});
        processEntry(entry, true, false);
        return entry;
    }

    private void processEntry(
            LongScreenshotsEntry entry,
            boolean skipAddingEntryToList,
            boolean addToBeginningOfList) {
        if (mGeneratorStatus == EntryStatus.CAPTURE_COMPLETE) {
            entry.generateBitmap();
        } else if (mGeneratorStatus == EntryStatus.CAPTURE_IN_PROGRESS) {
            mQueuedEntries.add(entry);
        } else {
            entry.updateStatus(mGeneratorStatus);
        }

        if (skipAddingEntryToList) return;

        // Add to the list of all entries
        if (addToBeginningOfList) {
            mEntries.add(0, entry);
        } else {
            mEntries.add(entry);
        }
    }

    /**
     * Updates based on the generator status. If the capture is complete, generates the bitmap for
     * all the queued entries.
     *
     * @param status New status from the generator.
     */
    private void updateGeneratorStatus(@EntryStatus int status) {
        mGeneratorStatus = status;

        if (status == EntryStatus.CAPTURE_COMPLETE) {
            for (LongScreenshotsEntry entry : mQueuedEntries) {
                entry.generateBitmap();
            }
            mQueuedEntries.clear();
        } else {
            for (LongScreenshotsEntry entry : mQueuedEntries) {
                entry.updateStatus(status);
            }
        }

        for (BitmapGeneratorObserver observer : mGeneratorObservers) {
            observer.onStatusChange(mGeneratorStatus);
        }
    }

    public void addBitmapGeneratorObserver(BitmapGeneratorObserver observer) {
        mGeneratorObservers.addObserver(observer);

        observer.onStatusChange(mGeneratorStatus);
        if (mGeneratorStatus == EntryStatus.CAPTURE_COMPLETE) {
            observer.onCompositorReady(mGenerator.getContentSize(), mGenerator.getScrollOffset());
        }
    }

    public void removeBitmapGeneratorObserver(BitmapGeneratorObserver observer) {
        mGeneratorObservers.removeObserver(observer);
    }

    /**
     * Creates the default BitmapGenerator to be used to retrieve the state of the generation. This
     * is the default implementation and should only be overridden for tests.
     */
    @VisibleForTesting
    public BitmapGenerator.GeneratorCallBack createBitmapGeneratorCallback() {
        return new BitmapGenerator.GeneratorCallBack() {
            @Override
            public void onCompositorResult(@CompositorStatus int status) {
                if (status == CompositorStatus.STOPPED_DUE_TO_MEMORY_PRESSURE
                        || status == CompositorStatus.SKIPPED_DUE_TO_MEMORY_PRESSURE) {
                    updateGeneratorStatus(EntryStatus.INSUFFICIENT_MEMORY);
                } else if (status == CompositorStatus.OK) {
                    updateGeneratorStatus(EntryStatus.CAPTURE_COMPLETE);

                    Size contentSize = mGenerator.getContentSize();
                    Point scrollOffset = mGenerator.getScrollOffset();
                    for (BitmapGeneratorObserver observer : mGeneratorObservers) {
                        observer.onCompositorReady(contentSize, scrollOffset);
                    }
                } else {
                    updateGeneratorStatus(EntryStatus.GENERATION_ERROR);
                }
            }

            @Override
            public void onCaptureResult(@Status int status) {
                if (status == Status.LOW_MEMORY_DETECTED) {
                    updateGeneratorStatus(EntryStatus.INSUFFICIENT_MEMORY);
                } else if (status != Status.OK) {
                    updateGeneratorStatus(EntryStatus.GENERATION_ERROR);
                }
            }
        };
    }

    public void destroy() {
        if (mGenerator != null) {
            mGenerator.destroy();
            mGenerator = null;
        }
    }
}
