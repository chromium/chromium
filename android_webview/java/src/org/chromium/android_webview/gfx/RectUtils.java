// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import android.graphics.Rect;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.List;

/** Utility functions for calculating Rectangle properties (i.e. Area of a single Rect) */
public final class RectUtils {
    private RectUtils() {}

    public static int getRectArea(Rect rect) {
        return rect.width() * rect.height();
    }

    /** Segment Type Constants */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({SegmentType.START, SegmentType.END})
    private @interface SegmentType {
        int START = 0;
        int END = 1;
    }

    private static int compareSegmentTypes(int s1, int s2) {
        if (s1 == s2) {
            return 0;
        } else if (s1 == SegmentType.START && s2 == SegmentType.END) {
            return -1;
        } else {
            return 1;
        }
    }

    private static class HorizontalSegment implements Comparable<HorizontalSegment> {
        public int mX;
        public int mTop;
        public int mBottom;
        public @SegmentType int mSegmentType;

        public HorizontalSegment() {
            set(0, 0, 0, SegmentType.START);
        }

        public void set(int x, int top, int bottom, @SegmentType int segmentType) {
            this.mX = x;
            this.mTop = top;
            this.mBottom = bottom;
            this.mSegmentType = segmentType;
        }

        @Override
        public int compareTo(HorizontalSegment other) {
            if (mX == other.mX) {
                return compareSegmentTypes(mSegmentType, other.mSegmentType);
            }
            return mX - other.mX;
        }
    }

    private static class VerticalSegment implements Comparable<VerticalSegment> {
        public int mY;
        public @SegmentType int mSegmentType;

        public VerticalSegment() {
            set(0, SegmentType.START);
        }

        public void set(int y, @SegmentType int segmentType) {
            this.mY = y;
            this.mSegmentType = segmentType;
        }

        public void set(VerticalSegment other) {
            set(other.mY, other.mSegmentType);
        }

        @Override
        public int compareTo(VerticalSegment other) {
            if (mY == other.mY) {
                return compareSegmentTypes(mSegmentType, other.mSegmentType);
            }
            return mY - other.mY;
        }
    }

    private static void insertSorted(
            VerticalSegment arr[], int n, VerticalSegment verticalSegment, int capacity) {
        assert n < capacity;

        int i;
        for (i = n - 1; (i >= 0 && arr[i].compareTo(verticalSegment) > 0); i--) {
            arr[i + 1].set(arr[i]);
        }

        int insert_index = i + 1;
        assert insert_index >= 0 && insert_index < capacity;
        arr[insert_index].set(verticalSegment);
    }

    private static int deleteElement(
            VerticalSegment arr[], int n, VerticalSegment verticalSegment) {
        int pos = Arrays.binarySearch(arr, 0, n, verticalSegment);
        if (pos < 0) {
            return -1;
        }

        // In the case of duplicate values, either one can be removed
        for (int i = pos + 1; i < n; i++) {
            arr[i - 1].set(arr[i]);
        }
        return n - 1;
    }

    private static int getCoverageOfVerticalSegments(
            VerticalSegment vSegments[], int numVerticalSegments) {
        int scanCount = 0;
        int coveredPixels = 0;
        int start = -1;

        for (int i = 0; i < numVerticalSegments; i++) {
            VerticalSegment verticalSegment = vSegments[i];
            if (scanCount == 0 && verticalSegment.mSegmentType == SegmentType.START) {
                start = verticalSegment.mY;
            } else if (scanCount == 1 && verticalSegment.mSegmentType == SegmentType.END) {
                coveredPixels += verticalSegment.mY - start;
            }
            scanCount += verticalSegment.mSegmentType == SegmentType.START ? 1 : -1;
        }
        return coveredPixels;
    }

    private static HorizontalSegment sHorizontalSegments[];
    private static VerticalSegment sVerticalSegments[];
    private static VerticalSegment sVerticalSegment1 = new VerticalSegment();
    private static VerticalSegment sVerticalSegment2 = new VerticalSegment();
    private static Rect sClippedRects[];

    /*
            This is a 2d extension of the 1d range intersection problem.
            In one dimension we are interested in calculating the
            intersected set of ranges for an input. To do this we decompose
            each input range into a start and an end position, plus whether
            it is entering a range or leaving it. Once these decomposed
            positions are sorted, we can compute the intersection by
            iterating over the list and recording transitions from not being
            in a range to being in a range, and vice versa.

            E.g. [1,4] U [2,5] U [7,9] -> [1,+1] [2,+1] [4,-1] [5,-1]
            [7,+1] [9,-1]. Then, summing the second component as we
            traverse, and looking for 0->1 and 1->0 transitions, we end up
            finding the union ranges [1,5], [7,9]

            In order to extend this to 2d axis aligned rectangles, we
            decompose rectangles into top and bottom edges that add or
            remove a range from the 1d data data structure. Before we add or
            remove a range to the 1d data structure we accumulate area equal
            to the current 1d coverage multiplied by the delta-y from the
            last point at which we updated the coverage.

    1  4  7   11 14  18
    1     +------+         [4,+1], [11,-1] cov=7 area += 0
    2  +----------------+  [1,+1], [4,+1], [11,-1], [18,-1], cov=17, rea += 7*1
    3  |  |  +------+   |  [1,+1], [4,+1], [7,+1], [11,-1], [14,-1], [18,-1], cov=17 area += 17*1
    4  |  +------+  |   |  [1,+1], [7,+1], [14,-1], [18,-1], cov=17 area += 17*1
    5  +----------------+  [7,+1], [14,-1] cov=7 area += 17*1
    6        +------+      [] area += 7*1
        */

    public static int calculatePixelsOfCoverage(Rect screenRect, List<Rect> coverageRects) {
        if (coverageRects.size() == 0) {
            return 0;
        }

        // Always allocate enough space for all passed rects and never trim allocations as a result
        // of clipping
        if ((sClippedRects == null ? 0 : sClippedRects.length) < coverageRects.size()) {
            sClippedRects = new Rect[coverageRects.size()];
        }

        int numClippedRects = 0;
        for (int i = 0; i < coverageRects.size(); i++) {
            Rect clipRect = coverageRects.get(i);
            if (clipRect.intersect(screenRect)) { // This line may modify the value of the passed
                // in coverage rects
                sClippedRects[numClippedRects++] = clipRect;
            }
        }

        if (numClippedRects == 0) {
            return 0;
        }

        int maxSegments = numClippedRects * 2;
        int numVerticalSegments = 0;

        if ((sHorizontalSegments == null ? 0 : sHorizontalSegments.length) < maxSegments) {
            sHorizontalSegments = new HorizontalSegment[maxSegments];
            sVerticalSegments = new VerticalSegment[maxSegments];

            for (int i = 0; i < maxSegments; i++) {
                sHorizontalSegments[i] = new HorizontalSegment();
                sVerticalSegments[i] = new VerticalSegment();
            }
        }

        for (int i = 0; i < maxSegments; i += 2) {
            Rect coverageRect = sClippedRects[i / 2];
            sHorizontalSegments[i].set(
                    coverageRect.left, coverageRect.top, coverageRect.bottom, SegmentType.START);
            sHorizontalSegments[i + 1].set(
                    coverageRect.right, coverageRect.top, coverageRect.bottom, SegmentType.END);
        }

        Arrays.sort(sHorizontalSegments, 0, maxSegments);

        int prev_x = -1;
        int coveredPixels = 0;
        for (int i = 0; i < maxSegments; i++) {
            HorizontalSegment hSegment = sHorizontalSegments[i];
            coveredPixels +=
                    getCoverageOfVerticalSegments(sVerticalSegments, numVerticalSegments)
                            * (hSegment.mX - prev_x);
            sVerticalSegment1.set(hSegment.mTop, SegmentType.START);
            sVerticalSegment2.set(hSegment.mBottom, SegmentType.END);

            if (hSegment.mSegmentType == SegmentType.START) {
                insertSorted(
                        sVerticalSegments, numVerticalSegments, sVerticalSegment1, maxSegments);
                numVerticalSegments++;
                insertSorted(
                        sVerticalSegments, numVerticalSegments, sVerticalSegment2, maxSegments);
                numVerticalSegments++;
            } else {
                int ret;
                ret = deleteElement(sVerticalSegments, numVerticalSegments, sVerticalSegment1);
                assert ret != -1;
                numVerticalSegments = ret;
                ret = deleteElement(sVerticalSegments, numVerticalSegments, sVerticalSegment2);
                assert ret != -1;
                numVerticalSegments = ret;
            }
            prev_x = hSegment.mX;
        }

        return coveredPixels;
    }
}
