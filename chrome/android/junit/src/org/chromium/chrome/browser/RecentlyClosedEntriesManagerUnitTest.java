// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package chrome.android.junit.src.org.chromium.chrome.browser;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.RecentlyClosedEntriesManagerTrackerFactory;
import org.chromium.chrome.browser.RecentlyClosedEntriesManagerTrackerImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
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

    private RecentlyClosedEntriesManager mRecentlyClosedEntriesManager;
    private final CallbackHelper mEntriesUpdatedCallbackHelper = new CallbackHelper();

    @Before
    public void setup() {
        RecentlyClosedEntriesManager.setRecentlyClosedTabManagerForTests(mRecentlyClosedTabManager);
        when(mTabModelSelector.getModel(/* incognito= */ false)).thenReturn(mTabModel);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        mRecentlyClosedEntriesManager =
                RecentlyClosedEntriesManagerTrackerFactory.getInstance()
                        .obtainManager(mMultiInstanceManager, mTabModelSelector);
        mRecentlyClosedEntriesManager.setEntriesUpdatedCallback(
                result -> mEntriesUpdatedCallbackHelper.notifyCalled());
    }

    @After
    public void teardown() {
        ((RecentlyClosedEntriesManagerTrackerImpl)
                        RecentlyClosedEntriesManagerTrackerFactory.getInstance())
                .setOpenMostRecentTabEntryNext(false);
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

        // Verify the excess window entries are cleaned up.
        ArgumentCaptor<List<Integer>> listCaptor = ArgumentCaptor.forClass(List.class);
        verify(mMultiInstanceManager)
                .closeWindows(listCaptor.capture(), eq(CloseWindowAppSource.RECENT_TABS));
        assertEquals(numWindowEntries, listCaptor.getValue().size());

        // Verify the excess session entries are cleaned up.
        verify(mRecentlyClosedTabManager)
                .clearLeastRecentlyUsedClosedEntries(eq(numSessionEntries));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
    public void testOpenMostRecentlyClosedEntry_FeatureDisabled() {
        when(mTabModel.getMostRecentClosureTime()).thenReturn(-1L);
        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(NewWindowAppSource.OTHER);
        verify(mTabModel).openMostRecentlyClosedEntry();
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_NoEntries() {
        when(mTabModel.getMostRecentClosureTime()).thenReturn(-1L);
        when(mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE))
                .thenReturn(new ArrayList<>());

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(NewWindowAppSource.OTHER);
        verify(mTabModel, never()).openMostRecentlyClosedEntry();
        verify(mMultiInstanceManager, never()).openWindow(anyInt(), anyInt());
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_NoWindowEntries() {
        when(mTabModel.getMostRecentClosureTime()).thenReturn(2L);
        when(mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE))
                .thenReturn(new ArrayList<>());

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(NewWindowAppSource.OTHER);
        verify(mTabModel).openMostRecentlyClosedEntry();
        verify(mMultiInstanceManager, never()).openWindow(anyInt(), anyInt());
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_NoTabEntries() {
        when(mTabModel.getMostRecentClosureTime()).thenReturn(-1L);
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ 0,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 3,
                        /* markedForDeletion= */ true));
        when(mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE))
                .thenReturn(instanceInfoList);

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);

        verify(mTabModel, never()).openMostRecentlyClosedEntry();
        verify(mMultiInstanceManager).openWindow(2, NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_MostRecentIsTab() {
        // Make tab closure entry more recent.
        when(mTabModel.getMostRecentClosureTime()).thenReturn(3L);
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ 0,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 2,
                        /* markedForDeletion= */ true));
        when(mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE))
                .thenReturn(instanceInfoList);

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(NewWindowAppSource.OTHER);
        verify(mTabModel).openMostRecentlyClosedEntry();
        verify(mMultiInstanceManager, never()).openWindow(anyInt(), anyInt());
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_MostRecentIsWindow_CanOpenWindow() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        MultiWindowUtils.setMaxInstancesForTesting(3);

        // Make window entry more recent.
        when(mTabModel.getMostRecentClosureTime()).thenReturn(2L);
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ 0,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 3,
                        /* markedForDeletion= */ true));
        when(mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE))
                .thenReturn(instanceInfoList);

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);
        verify(mMultiInstanceManager)
                .openWindow(2, MultiInstanceManager.NewWindowAppSource.KEYBOARD_SHORTCUT);
        verify(mTabModel, never()).openMostRecentlyClosedEntry();
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_MostRecentIsWindow_MaxInstances() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        MultiWindowUtils.setMaxInstancesForTesting(3);

        // Make window entry more recent.
        when(mTabModel.getMostRecentClosureTime()).thenReturn(2L);
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ 0,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 3,
                        /* markedForDeletion= */ true));
        when(mMultiInstanceManager.getInstanceInfo(PersistedInstanceType.INACTIVE))
                .thenReturn(instanceInfoList);

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(NewWindowAppSource.OTHER);
        verify(mTabModel).openMostRecentlyClosedEntry();
        verify(mMultiInstanceManager, never()).openWindow(anyInt(), anyInt());
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_TabClosureTimeIsZero() {
        MultiWindowUtils.setInstanceCountForTesting(2);
        MultiWindowUtils.setMaxInstancesForTesting(3);

        // Assume that we have two closed windows, and tab entries with closure timestamp 0.
        when(mTabModel.getMostRecentClosureTime()).thenReturn(0L);
        createRecentlyClosedWindows(/* numOfWindows= */ 2);

        // First request should open the most recently closed window.
        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);
        // Next request should open the tab entry with timestamp 0.
        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);
        // Next request should open the next most recently closed window.
        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);

        // Verify that most recent entries are opened in the expected order.
        InOrder inOrder = inOrder(mMultiInstanceManager, mTabModel);
        inOrder.verify(mMultiInstanceManager)
                .openWindow(
                        anyInt(), eq(MultiInstanceManager.NewWindowAppSource.KEYBOARD_SHORTCUT));
        inOrder.verify(mTabModel).openMostRecentlyClosedEntry();
        inOrder.verify(mMultiInstanceManager)
                .openWindow(
                        anyInt(), eq(MultiInstanceManager.NewWindowAppSource.KEYBOARD_SHORTCUT));
    }

    @Test
    public void testOpenRecentlyClosedEntry_MaxInstances_ShowInstanceCreationLimitMessage() {
        // Simulate reaching the instance limit.
        MultiWindowUtils.setInstanceCountForTesting(3);
        MultiWindowUtils.setMaxInstancesForTesting(3);

        // Open recently closed window.
        RecentlyClosedWindow window =
                new RecentlyClosedWindow(
                        /* timestamp= */ 10,
                        /* instanceId= */ 4,
                        /* url= */ "url",
                        /* title= */ "title",
                        /* activeTabTitle= */ "tab title",
                        /* tabCount= */ 1);
        mRecentlyClosedEntriesManager.openRecentlyClosedEntry(window);

        // Verify window is not opened and the instance creation limit message is shown
        verify(mMultiInstanceManager, never()).openWindow(anyInt(), anyInt());
        verify(mMultiInstanceManager).showInstanceCreationLimitMessage();
    }

    @Test
    public void testOnWindowClosed_NotPermanentDeletion_AddsWindow() {
        createRecentlyClosedWindows(/* numOfWindows= */ 1);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();
        RecentlyClosedWindow window =
                new RecentlyClosedWindow(
                        /* timestamp= */ 10,
                        /* instanceId= */ 4,
                        /* url= */ "url",
                        /* title= */ "title",
                        /* activeTabTitle= */ "tab title",
                        /* tabCount= */ 1);
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        mRecentlyClosedEntriesManager.onWindowClosed(window, /* isPermanentDeletion= */ false);

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(2, entries.size());
        assertEquals(window, entries.get(0));
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowClosed_PermanentDeletion_RemovesWindow() {
        RecentlyClosedWindow window =
                new RecentlyClosedWindow(
                        /* timestamp= */ 10,
                        /* instanceId= */ 2,
                        /* url= */ "url",
                        /* title= */ "title",
                        /* activeTabTitle= */ "tab title",
                        /* tabCount= */ 1);
        mRecentlyClosedEntriesManager.onWindowClosed(window, /* isPermanentDeletion= */ false);
        assertEquals(1, mRecentlyClosedEntriesManager.getRecentlyClosedEntries().size());
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        mRecentlyClosedEntriesManager.onWindowClosed(window, /* isPermanentDeletion= */ true);

        assertEquals(0, mRecentlyClosedEntriesManager.getRecentlyClosedEntries().size());
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowClosed_MovesExistingWindowToTop() {
        createRecentlyClosedWindows(/* numOfWindows= */ 2);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        RecentlyClosedWindow olderWindow = (RecentlyClosedWindow) entries.get(1);
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        mRecentlyClosedEntriesManager.onWindowClosed(olderWindow, /* isPermanentDeletion= */ false);

        entries = mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(2, entries.size());
        assertEquals(olderWindow, entries.get(0));
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowClosed_ExceedsMaxEntries_ClearWindowStorage() {
        int size = 25;
        createRecentlyClosedWindows(/* numOfWindows= */ 25);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();

        assertEquals(size, entries.size());
        RecentlyClosedWindow newWindow =
                new RecentlyClosedWindow(
                        /* timestamp= */ 10,
                        /* instanceId= */ 1,
                        /* url= */ "url",
                        /* title= */ "title",
                        /* activeTabTitle= */ "tab title",
                        /* tabCount= */ 1);
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        // Verify the excess window entry is cleaned up from storage.
        mRecentlyClosedEntriesManager.onWindowClosed(newWindow, /* isPermanentDeletion= */ false);
        ArgumentCaptor<List<Integer>> listCaptor = ArgumentCaptor.forClass(List.class);
        verify(mMultiInstanceManager)
                .closeWindows(listCaptor.capture(), eq(CloseWindowAppSource.RECENT_TABS));
        assertEquals(1, listCaptor.getValue().size());
        verify(mRecentlyClosedTabManager, never()).clearLeastRecentlyUsedClosedEntries(anyInt());

        entries = mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(25, entries.size());
        assertEquals(newWindow, entries.get(0));
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowClosed_ExceedsMaxEntries_ClearSessionEntryStorage() {
        int size = 25;
        createRecentlyClosedWindows(/* numOfWindows= */ 15);
        createSessionRecentlyClosedEntries(/* numOfEntries= */ 10);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();

        assertEquals(size, entries.size());
        RecentlyClosedWindow newWindow =
                new RecentlyClosedWindow(
                        /* timestamp= */ 10,
                        /* instanceId= */ 1,
                        /* url= */ "url",
                        /* title= */ "title",
                        /* activeTabTitle= */ "tab title",
                        /* tabCount= */ 1);
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        // Verify the excess session entry is cleaned up from storage.
        mRecentlyClosedEntriesManager.onWindowClosed(newWindow, /* isPermanentDeletion= */ false);
        verify(mMultiInstanceManager, never()).closeWindows(any(), anyInt());
        verify(mRecentlyClosedTabManager).clearLeastRecentlyUsedClosedEntries(eq(1));

        entries = mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(25, entries.size());
        assertEquals(newWindow, entries.get(0));
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowRestored_RemovesEntry() {
        createRecentlyClosedWindows(/* numOfWindows= */ 1);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(1, entries.size());
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        mRecentlyClosedEntriesManager.onWindowRestored(/* instanceId= */ 2);

        assertEquals(0, mRecentlyClosedEntriesManager.getRecentlyClosedEntries().size());
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_TabTimestampZero_WindowNewerThanNextTab() {
        // Test merging when a tab has a timestamp of 0 and the window is newer than the next tab
        // with a valid timestamp. The window should be prioritized.
        createRecentlyClosedWindows(/* numOfWindows= */ 1); // timestamp = 2
        List<RecentlyClosedEntry> sessionEntries = new ArrayList<>();
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 1, /* timestamp= */ 0));
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 2, /* timestamp= */ 1));
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt()))
                .thenReturn(sessionEntries);

        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(3, entries.size());
        assertTrue(entries.get(0) instanceof RecentlyClosedWindow);
        assertTrue(entries.get(1) instanceof SessionRecentlyClosedEntry);
        assertEquals(1, ((SessionRecentlyClosedEntry) entries.get(1)).getSessionId());
        assertTrue(entries.get(2) instanceof SessionRecentlyClosedEntry);
        assertEquals(2, ((SessionRecentlyClosedEntry) entries.get(2)).getSessionId());
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_NextTwoTabTimestampsZero() {
        // Test merging when the next two tabs have timestamps of 0. The window should be
        // prioritized.
        createRecentlyClosedWindows(/* numOfWindows= */ 1); // timestamp = 2
        List<RecentlyClosedEntry> sessionEntries = new ArrayList<>();
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 1, /* timestamp= */ 0));
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 2, /* timestamp= */ 0));
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt()))
                .thenReturn(sessionEntries);

        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(3, entries.size());
        assertTrue(entries.get(0) instanceof RecentlyClosedWindow);
        assertTrue(entries.get(1) instanceof SessionRecentlyClosedEntry);
        assertEquals(1, ((SessionRecentlyClosedEntry) entries.get(1)).getSessionId());
        assertTrue(entries.get(2) instanceof SessionRecentlyClosedEntry);
        assertEquals(2, ((SessionRecentlyClosedEntry) entries.get(2)).getSessionId());
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_TabTimestampZero_NoNextTab() {
        // Test merging when a tab has a timestamp of 0 and there's no next tab. The window should
        // be prioritized, and the tab with timestamp 0 should be added if there's space.
        createRecentlyClosedWindows(/* numOfWindows= */ 1); // timestamp = 2
        List<RecentlyClosedEntry> sessionEntries = new ArrayList<>();
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 1, /* timestamp= */ 0));
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt()))
                .thenReturn(sessionEntries);

        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(2, entries.size());
        assertTrue(entries.get(0) instanceof RecentlyClosedWindow);
        assertTrue(entries.get(1) instanceof SessionRecentlyClosedEntry);
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_TabTimestampZero_NextTabNewerThanWindow() {
        // Test merging when a tab has a timestamp of 0 and the next tab is newer than the window.
        // The tab with timestamp 0 should be prioritized.
        createRecentlyClosedWindows(/* numOfWindows= */ 1); // timestamp = 2

        List<RecentlyClosedEntry> sessionEntries = new ArrayList<>();
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 1, /* timestamp= */ 0));
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 2, /* timestamp= */ 3));

        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt()))
                .thenReturn(sessionEntries);

        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(3, entries.size());
        assertTrue(entries.get(0) instanceof SessionRecentlyClosedEntry);
        assertEquals(1, ((SessionRecentlyClosedEntry) entries.get(0)).getSessionId());
        assertTrue(entries.get(1) instanceof SessionRecentlyClosedEntry);
        assertEquals(2, ((SessionRecentlyClosedEntry) entries.get(1)).getSessionId());
        assertTrue(entries.get(2) instanceof RecentlyClosedWindow);
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_TabTimestampZero_NoSpaceForTab() {
        // Test merging when a tab has a timestamp of 0 and there is no space to add it.
        // It should not be added.
        RecentlyClosedEntriesManager.setMaxEntriesForTests(3);
        createRecentlyClosedWindows(/* numOfWindows= */ 2);

        List<RecentlyClosedEntry> sessionEntries = new ArrayList<>();
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 1, /* timestamp= */ 3));
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 2, /* timestamp= */ 0));

        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt()))
                .thenReturn(sessionEntries);

        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();

        assertEquals(3, entries.size());
        assertTrue(entries.get(0) instanceof RecentlyClosedWindow);
        assertTrue(entries.get(1) instanceof SessionRecentlyClosedEntry);
        assertEquals(1, ((SessionRecentlyClosedEntry) entries.get(1)).getSessionId());
        assertTrue(entries.get(2) instanceof RecentlyClosedWindow);
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
