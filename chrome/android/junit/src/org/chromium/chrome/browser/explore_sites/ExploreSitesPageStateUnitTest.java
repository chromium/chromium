// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.os.Parcel;
import android.os.Parcelable;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit tests for {@link ExploreSitesPage.PageState}
 *
 * Tested Methods:
 *  - CREATOR.createFromParcel
 *  - writeToParcel
 *  - getLastTimestamp
 *  - getStableLayoutManagerState
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExploreSitesPageStateUnitTest {
    /* Tests that WriteToParcel and CreateFromParcel are inverses of each other. We expect this test
     * to fail if testGetLastTimeStamp or testGetStableLayoutManagerState fails.
     */
    @Test
    @SmallTest
    public void testWriteToParcelCreateFromParcelInverse() {
        Parcelable testParcelableData = new TestParcelable(78);
        long fakeTimestamp = 13;

        ExploreSitesPage.PageState pageState =
                new ExploreSitesPage.PageState(fakeTimestamp, testParcelableData);

        Parcel parcel = Parcel.obtain();

        pageState.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        ExploreSitesPage.PageState newPageState =
                ExploreSitesPage.PageState.CREATOR.createFromParcel(parcel);

        parcel.recycle();

        Assert.assertEquals(fakeTimestamp, newPageState.getLastTimestamp());
        Assert.assertEquals(testParcelableData, newPageState.getStableLayoutManagerState());
    }

    /* Tests that getting the last timeStamp from a PageState returns the timestamp passed to
     * constructor.
     */
    @Test
    @SmallTest
    public void testGetLastTimeStamp() {
        Parcelable testParcelableData = new TestParcelable(78);
        long fakeTimestamp = 13;

        ExploreSitesPage.PageState pageState =
                new ExploreSitesPage.PageState(fakeTimestamp, testParcelableData);

        Assert.assertEquals(fakeTimestamp, pageState.getLastTimestamp());
    }

    /* Tests that getting the parcelable (normally an instance of StableLayoutManager Saved State)
     * from a PageState returns the parcelable passed to constructor.
     */
    @Test
    @SmallTest
    public void testGetStableLayoutManagerState() {
        Parcelable testParcelableData = new TestParcelable(78);
        long fakeTimestamp = 13;

        ExploreSitesPage.PageState pageState =
                new ExploreSitesPage.PageState(fakeTimestamp, testParcelableData);

        Assert.assertEquals(testParcelableData, pageState.getStableLayoutManagerState());
    }

    /** A parcelable object that can be passed to PageState */
    private static class TestParcelable implements Parcelable {
        int mContents;

        public TestParcelable(int contents) {
            this.mContents = contents;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(mContents);
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof TestParcelable)) return false;
            return mContents == ((TestParcelable) other).mContents;
        }

        public static final Creator<TestParcelable> CREATOR = new Creator<TestParcelable>() {
            @Override
            public TestParcelable createFromParcel(Parcel source) {
                return new TestParcelable(source.readInt());
            }

            @Override
            public TestParcelable[] newArray(int size) {
                return new TestParcelable[0];
            }
        };
    }
}
