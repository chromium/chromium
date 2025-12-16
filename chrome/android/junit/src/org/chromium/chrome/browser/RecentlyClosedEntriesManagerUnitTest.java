// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package chrome.android.junit.src.org.chromium.chrome.browser;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedTabManager;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.ntp.SessionRecentlyClosedEntry;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link RecentlyClosedEntriesManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
public class RecentlyClosedEntriesManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private static final int RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW = 25;
    @Mock MultiInstanceManager mMultiInstanceManager;
    @Mock RecentlyClosedTabManager mRecentlyClosedTabManager;
    @Mock TabModelSelector mTabModelSelector;
    @Mock TabModel mTabModel;
    @Mock Profile mProfile;
    RecentlyClosedEntriesManager mRecentlyClosedEntriesManager;

    @Before
    public void setup() {
        RecentlyClosedEntriesManager.setRecentlyClosedTabManagerForTests(mRecentlyClosedTabManager);
        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        mRecentlyClosedEntriesManager =
                new RecentlyClosedEntriesManager(mMultiInstanceManager, mTabModelSelector);
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int numSessionEntries = 5;
        int numWindowEntries = 5;
        int totalEntries = numSessionEntries + numWindowEntries;
        createRecentlyClosedWindows(numSessionEntries);
        createSessionRecentlyClosedEntries(numWindowEntries);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(totalEntries, entries.size());
        for (int i = 0; i < entries.size(); i++) {
            RecentlyClosedEntry entry = entries.get(i);
            // Assert odd indices should be session entries; even should be window entries;
            if (i % 2 != 0) {
                assertTrue(
                        "Index " + i + " should be a SessionRecentlyClosedEntry.",
                        entry instanceof SessionRecentlyClosedEntry);
            } else {
                assertTrue(
                        "Index " + i + " should be a RecentlyClosedWindow.",
                        entry instanceof RecentlyClosedWindow);
            }
            // Assert timestamps strictly decrease.
            if (i > 0) {
                assertThat(
                        "The entries should be sorted by timestamp",
                        entries.get(i - 1).getDate().getTime(),
                        greaterThan(entry.getDate().getTime()));
            }
        }
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_SessionEntriesNull() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int totalEntries = 5;
        createRecentlyClosedWindows(/* numOfWindows= */ totalEntries);
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt())).thenReturn(null);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(totalEntries, entries.size());
        for (int i = 0; i < entries.size(); i++) {
            RecentlyClosedEntry entry = entries.get(i);
            // Assert that it should only contain window entries;
            assertTrue(
                    "Index " + i + " should be a RecentlyClosedWindow.",
                    entry instanceof RecentlyClosedWindow);

            // Assert timestamps strictly decrease.
            if (i > 0) {
                assertThat(
                        "The entries should be sorted by timestamp",
                        entries.get(i - 1).getDate().getTime(),
                        greaterThan(entry.getDate().getTime()));
            }
        }
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_SessionEntriesNull_WindowEntriesEmpty() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int totalEntries = 0;
        when(mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE))
                .thenReturn(null);
        createRecentlyClosedWindows(/* numOfWindows= */ totalEntries);
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt())).thenReturn(null);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size is 0.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(totalEntries, entries.size());
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_SessionEntriesEmpty() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int totalEntries = 5;
        createRecentlyClosedWindows(/* numOfWindows= */ totalEntries);
        createSessionRecentlyClosedEntries(/* numOfEntries= */ 0);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(totalEntries, entries.size());
        for (int i = 0; i < entries.size(); i++) {
            RecentlyClosedEntry entry = entries.get(i);
            // Assert that it should only contain window entries;
            assertTrue(
                    "Index " + i + " should be a RecentlyClosedWindow.",
                    entry instanceof RecentlyClosedWindow);

            // Assert timestamps strictly decrease.
            if (i > 0) {
                assertThat(
                        "The entries should be sorted by timestamp",
                        entries.get(i - 1).getDate().getTime(),
                        greaterThan(entry.getDate().getTime()));
            }
        }
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_WindowEntriesEmpty() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int totalEntries = 5;
        createRecentlyClosedWindows(/* numOfWindows= */ 0);
        createSessionRecentlyClosedEntries(/* numOfEntries= */ totalEntries);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(totalEntries, entries.size());
        for (int i = 0; i < entries.size(); i++) {
            RecentlyClosedEntry entry = entries.get(i);
            // Assert that it should only contain session entries;
            assertTrue(
                    "Index " + i + " should be a SessionRecentlyClosedEntry.",
                    entry instanceof SessionRecentlyClosedEntry);

            // Assert timestamps strictly decrease.
            if (i > 0) {
                assertThat(
                        "The entries should be sorted by timestamp",
                        entries.get(i - 1).getDate().getTime(),
                        greaterThan(entry.getDate().getTime()));
            }
        }
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_BothEntriesEmpty() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int totalEntries = 0;
        createRecentlyClosedWindows(/* numOfWindows= */ 0);
        createSessionRecentlyClosedEntries(/* numOfEntries= */ 0);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(totalEntries, entries.size());
        for (int i = 0; i < entries.size(); i++) {
            RecentlyClosedEntry entry = entries.get(i);
            // Assert that it should only contain session entries;
            assertTrue(
                    "Index " + i + " should be a SessionRecentlyClosedEntry.",
                    entry instanceof SessionRecentlyClosedEntry);

            // Assert timestamps strictly decrease.
            if (i > 0) {
                assertThat(
                        "The entries should be sorted by timestamp",
                        entries.get(i - 1).getDate().getTime(),
                        greaterThan(entry.getDate().getTime()));
            }
        }
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_SessionEntriesGreaterThanWindowEntries() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int numWindowEntries = 6;
        int numSessionEntries = 8;
        int totalEntries = numWindowEntries + numSessionEntries;
        createRecentlyClosedWindows(numWindowEntries);
        createSessionRecentlyClosedEntries(numSessionEntries);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(totalEntries, entries.size());
        for (int i = entries.size() - 1; i >= 0; i--) {
            RecentlyClosedEntry entry = entries.get(i);
            // Assert odd indices should be session entries; even should be window entries;
            if (i % 2 != 0 || numWindowEntries == 0) {
                assertTrue(
                        "Index " + i + " should be a SessionRecentlyClosedEntry.",
                        entry instanceof SessionRecentlyClosedEntry);
            } else if (numWindowEntries > 0) {
                assertTrue(
                        "Index " + i + " should be a RecentlyClosedWindow.",
                        entry instanceof RecentlyClosedWindow);
                numWindowEntries--;
            }
            // Assert timestamps strictly decrease.
            if (i > 0) {
                assertThat(
                        "The entries should be sorted by timestamp",
                        entries.get(i - 1).getDate().getTime(),
                        greaterThan(entry.getDate().getTime()));
            }
        }
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_WindowEntriesGreaterThanSessionEntries() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int numWindowEntries = 10;
        int numSessionEntries = 5;
        int totalEntries = numWindowEntries + numSessionEntries;
        createRecentlyClosedWindows(numWindowEntries);
        createSessionRecentlyClosedEntries(numSessionEntries);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(totalEntries, entries.size());
        for (int i = entries.size() - 1; i >= 0; i--) {
            RecentlyClosedEntry entry = entries.get(i);
            // Assert odd indices should be session entries; even should be window entries;
            if (i % 2 == 0 && numSessionEntries > 0) {
                assertTrue(
                        "Index " + i + " should be a SessionRecentlyClosedEntry.",
                        entry instanceof SessionRecentlyClosedEntry);
                numSessionEntries--;
            } else {
                assertTrue(
                        "Index " + i + " should be a RecentlyClosedWindow.",
                        entry instanceof RecentlyClosedWindow);
            }
            // Assert timestamps strictly decrease.
            if (i > 0) {
                assertThat(
                        "The entries should be sorted by timestamp",
                        entries.get(i - 1).getDate().getTime(),
                        greaterThan(entry.getDate().getTime()));
            }
        }
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_ExceedsMaxEntries() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int numWindowEntries = 15;
        int numSessionEntries = 15;
        int maxEntries = mRecentlyClosedEntriesManager.getRecentlyClosedMaxEntry();
        assertEquals(RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW, maxEntries);
        createRecentlyClosedWindows(numWindowEntries);
        createSessionRecentlyClosedEntries(numSessionEntries);

        // This method should merge both lists into a single, timestamp-sorted list.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size is clamp by the maxEntries size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(maxEntries, entries.size());
        for (int i = entries.size() - 1; i >= 0; i--) {
            RecentlyClosedEntry entry = entries.get(i);
            // Assert odd indices should be session entries; even should be window entries;
            if (i % 2 != 0) {
                assertTrue(
                        "Index " + i + " should be a SessionRecentlyClosedEntry.",
                        entry instanceof SessionRecentlyClosedEntry);
                numSessionEntries--;
            } else {
                assertTrue(
                        "Index " + i + " should be a RecentlyClosedEntryWindow.",
                        entry instanceof RecentlyClosedWindow);
            }
            // Assert timestamps strictly decrease.
            if (i > 0) {
                assertThat(
                        "The entries should be sorted by timestamp",
                        entries.get(i - 1).getDate().getTime(),
                        greaterThan(entry.getDate().getTime()));
            }
        }
    }

    /**
     * Creates a list of synthetic {@code SessionRecentlyClosedEntry} objects for testing purposes.
     *
     * <p>The entries are assigned mock session IDs and timestamps, which are incremented by 2 for
     * each subsequent entry. The entries are added to the list in descending chronological order,
     * with odd-numbered timestamps (1, 3, 5, etc.) to allow for easy interleaving with window
     * entries. To easily test interleaving using the odd/even timestamp pattern, the number of
     * session entries and window entries should be equal.
     *
     * @param numOfEntries The number of session entries to create.
     */
    private void createSessionRecentlyClosedEntries(int numOfEntries) {
        List<RecentlyClosedEntry> sessionEntries = new ArrayList<>();
        int timestamp = 1;
        int sessionId = 1;
        for (int i = 0; i < numOfEntries; i++) {
            sessionEntries.add(0, new SessionRecentlyClosedEntry(sessionId, timestamp));
            sessionId += 2;
            timestamp += 2;
        }
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt()))
                .thenReturn(sessionEntries);
    }

    /**
     * Creates a list of synthetic {@Link InstanceInfo} objects that will be used to created as
     * {@Code RecentlyClosedWindow} for testing purpose.
     *
     * <p>The instances are assigned mock instance IDs and timestamps, which are incremented by 2
     * for each subsequent entry. The entries are added to the list in descending chronological
     * order, with even-numbered timestamp (2, 4, 6, etc.) to allow for easy interleaving with
     * session entries. For easy testing of the interleaving using the odd/even timestamp pattern,
     * the number of session entries and window entries should be equal.
     *
     * @param numOfWindows The number of window entries to create.
     */
    private void createRecentlyClosedWindows(int numOfWindows) {
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        int lastAccessedTime = 2;
        int instanceId = 2;
        for (int i = 0; i < numOfWindows; i++) {
            InstanceInfo instanceInfo =
                    new InstanceInfo(
                            instanceId,
                            /* taskId= */ 0,
                            InstanceInfo.Type.OTHER,
                            /* url= */ "",
                            /* title= */ "",
                            /* customTitle= */ null,
                            /* tabCount= */ 0,
                            /* incognitoTabCount= */ 0,
                            /* isIncognitoSelected= */ false,
                            lastAccessedTime,
                            /* markedForDeletion= */ true);
            instanceInfoList.add(0, instanceInfo);
            instanceId += 2;
            lastAccessedTime += 2;
        }
        when(mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE))
                .thenReturn(instanceInfoList);
    }
}
