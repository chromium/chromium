// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * Tests for the ActivityAssigner class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ActivityAssignerTest {
    private static final String BASE_WEBAPP_ID = "BASE_WEBAPP_ID_";

    private AdvancedMockContext mContext;
    private HashMap<String, Object>[] mPreferences;

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    @Before
    @SuppressWarnings("unchecked")
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);
        mContext = new AdvancedMockContext(ContextUtils.getApplicationContext());
        mPreferences = new HashMap[ActivityAssigner.ActivityAssignerNamespace.NUM_ENTRIES];
        for (int i = 0; i < ActivityAssigner.ActivityAssignerNamespace.NUM_ENTRIES; ++i) {
            mPreferences[i] = new HashMap<String, Object>();
            mContext.addSharedPreferences(ActivityAssigner.PREF_PACKAGE[i], mPreferences[i]);
        }
        ContextUtils.initApplicationContextForTests(mContext);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testEntriesCreated() {
        ActivityAssigner assigner = ActivityAssigner.instance(
                ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);

        // Make sure that no webapps have been assigned to any Activities for a fresh install.
        checkState(assigner, ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);
        List<ActivityAssigner.ActivityEntry> entries = assigner.getEntries();
        Assert.assertEquals(ActivityAssigner.NUM_WEBAPP_ACTIVITIES, entries.size());
        for (ActivityAssigner.ActivityEntry entry : entries) {
            Assert.assertEquals(null, entry.mWebappId);
        }
    }

    /**
     * Make sure invalid entries get culled & that we still have the correct number of unique
     * Activity indices available.
     */
    @Test
    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testEntriesDownsized() {
        // Store preferences indicating that more Activities existed previously than there are now.
        int numSavedEntries = ActivityAssigner.NUM_WEBAPP_ACTIVITIES + 1;
        createPreferences(
                numSavedEntries, ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);

        ActivityAssigner assigner = ActivityAssigner.instance(
                ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);
        checkState(assigner, ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);
    }

    /**
     * Make sure we recover from corrupted stored preferences.
     */
    @Test
    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    @RetryOnFailure
    public void testCorruptedPreferences() {
        String wrongVariableType = "omgwtfbbq";
        int index = ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE;
        mPreferences[index].clear();
        mPreferences[index].put(ActivityAssigner.PREF_NUM_SAVED_ENTRIES[index], wrongVariableType);

        ActivityAssigner assigner = ActivityAssigner.instance(
                ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);
        checkState(assigner, ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    @Feature({"Webapps"})
    public void testAssignment() {
        ActivityAssigner assigner = ActivityAssigner.instance(
                ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);

        checkState(assigner, ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);

        // Assign all of the Activities to webapps.
        // Go backwards to make sure ordering doesn't matter.
        Map<String, Integer> testMap = new HashMap<String, Integer>();
        for (int i = ActivityAssigner.NUM_WEBAPP_ACTIVITIES - 1; i >= 0; --i) {
            String currentWebappId = BASE_WEBAPP_ID + i;
            int activityIndex = assigner.assign(currentWebappId);
            testMap.put(currentWebappId, activityIndex);
        }
        Assert.assertEquals(ActivityAssigner.NUM_WEBAPP_ACTIVITIES, testMap.size());

        // Make sure that passing in the same webapp ID gives back the same Activity.
        for (int i = 0; i < ActivityAssigner.NUM_WEBAPP_ACTIVITIES; ++i) {
            String currentWebappId = BASE_WEBAPP_ID + i;
            int actualIndex = assigner.assign(currentWebappId);
            int expectedIndex = testMap.get(currentWebappId);
            Assert.assertEquals(expectedIndex, actualIndex);
        }

        // Access all but the last one to ensure that the last Activity is recycled.
        for (int i = 0; i < ActivityAssigner.NUM_WEBAPP_ACTIVITIES - 1; ++i) {
            String currentWebappId = BASE_WEBAPP_ID + i;
            int activityIndex = testMap.get(currentWebappId);
            assigner.markActivityUsed(activityIndex, currentWebappId);
        }

        // Make sure that the least recently used Activity is repurposed when we run out.
        String overflowWebappId = "OVERFLOW_ID";
        int overflowActivityIndex = assigner.assign(overflowWebappId);

        String lastAssignedWebappId = BASE_WEBAPP_ID + (ActivityAssigner.NUM_WEBAPP_ACTIVITIES - 1);
        int lastAssignedCurrentActivityIndex = assigner.assign(lastAssignedWebappId);
        int lastAssignedPreviousActivityIndex = testMap.get(lastAssignedWebappId);

        Assert.assertEquals("Overflow webapp did not steal the Activity from the other webapp",
                lastAssignedPreviousActivityIndex, overflowActivityIndex);
        Assert.assertNotSame("Webapp did not get reassigned to a new Activity.",
                lastAssignedPreviousActivityIndex, lastAssignedCurrentActivityIndex);

        checkState(assigner, ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    @Feature({"WebApk"})
    public void testInstance() {
        Assert.assertNotSame(ActivityAssigner.instance(
                                     ActivityAssigner.ActivityAssignerNamespace.WEBAPP_NAMESPACE),
                ActivityAssigner.instance(
                        ActivityAssigner.ActivityAssignerNamespace.WEBAPK_NAMESPACE));
    }

    /** Saves state indicating that a number of WebappActivities have already been saved out. */
    private void createPreferences(int numSavedEntries, int activityTypeIndex) {
        mPreferences[activityTypeIndex].clear();
        mPreferences[activityTypeIndex].put(
                ActivityAssigner.PREF_NUM_SAVED_ENTRIES[activityTypeIndex], numSavedEntries);
        for (int i = 0; i < numSavedEntries; ++i) {
            String activityIndexKey =
                    ActivityAssigner.PREF_ACTIVITY_INDEX[activityTypeIndex] + i;
            mPreferences[activityTypeIndex].put(activityIndexKey, i);

            String webappIdKey = ActivityAssigner.PREF_ACTIVITY_ID[activityTypeIndex] + i;
            String webappIdValue = BASE_WEBAPP_ID + i;
            mPreferences[activityTypeIndex].put(webappIdKey, webappIdValue);
        }
    }

    /** Checks the saved state to make sure it makes sense. */
    private void checkState(ActivityAssigner assigner, int namespace) {
        List<ActivityAssigner.ActivityEntry> entries = assigner.getEntries();

        // Confirm that the right number of entries in memory and in the preferences.
        Assert.assertEquals(ActivityAssigner.NUM_WEBAPP_ACTIVITIES, entries.size());
        Assert.assertEquals(ActivityAssigner.NUM_WEBAPP_ACTIVITIES,
                (int) (Integer) mPreferences[namespace].get(
                        assigner.getPreferenceNumberOfSavedEntries()));

        // Confirm that the Activity indices go from 0 to NUM_WEBAPP_ACTIVITIES - 1.
        HashSet<Integer> assignedActivities = new HashSet<Integer>();
        for (ActivityAssigner.ActivityEntry entry : entries) {
            assignedActivities.add(entry.mActivityIndex);
        }
        for (int i = 0; i < ActivityAssigner.NUM_WEBAPP_ACTIVITIES; ++i) {
            Assert.assertTrue(assignedActivities.contains(i));
        }
    }
}
