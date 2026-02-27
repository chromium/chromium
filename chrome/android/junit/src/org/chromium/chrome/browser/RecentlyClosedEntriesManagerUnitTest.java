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
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.util.Pair;

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

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.TimeUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.RecentlyClosedEntriesManagerTrackerFactory;
import org.chromium.chrome.browser.RecentlyClosedEntriesManagerTrackerImpl;
import org.chromium.chrome.browser.RecentlyClosedWindowMetadata;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedTabManager;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.ntp.SessionRecentlyClosedEntry;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link RecentlyClosedEntriesManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
public class RecentlyClosedEntriesManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    private static final int RECENTLY_CLOSED_MAX_ENTRY_COUNT_WITH_WINDOW = 25;
    @Mock MultiInstanceManager mMultiInstanceManager;
    @Mock RecentlyClosedTabManager mRecentlyClosedTabManager;
    @Mock TabModelSelector mTabModelSelector;
    @Mock TabWindowManager mTabWindowManager;
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
        createRecentlyClosedWindows(numWindowEntries);
        createSessionRecentlyClosedEntries(numSessionEntries);

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
    public void testMergeRecentlyClosedEntriesWithWindow_ExpiredSessionEntry() {
        // Create a few windows closed within 6 months ago from now.
        createRecentlyClosedWindows(/* numOfWindows= */ 2);
        // Create a session entry with an expired retention period.
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt()))
                .thenReturn(
                        List.of(
                                new SessionRecentlyClosedEntry(
                                        /* sessionId= */ 1,
                                        getDaysAgoMillis(/* numDaysAgo= */ 185))));

        // This method should exclude the expired session entry.
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Assert merged size and ordering.
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(2, entries.size());
        assertTrue(
                "Index 0 should be a RecentlyClosedWindow.",
                entries.get(0) instanceof RecentlyClosedWindow);
        assertTrue(
                "Index 1 should be a RecentlyClosedWindow.",
                entries.get(1) instanceof RecentlyClosedWindow);
    }

    @Test
    public void testMergeRecentlyClosedEntriesWithWindow_SessionEntriesNull_WindowEntriesEmpty() {
        // Create two separate lists (windows and session entries) with chronologically interleaved
        // timestamps.
        int totalEntries = 0;
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(null);
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
        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);
        verify(mTabModel).openMostRecentlyClosedEntry();
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_NoEntries() {
        when(mTabModel.getMostRecentClosureTime()).thenReturn(-1L);
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(new ArrayList<>());

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);
        verify(mTabModel, never()).openMostRecentlyClosedEntry();
        verify(mMultiInstanceManager, never()).openWindow(anyInt(), anyInt());
    }

    @Test
    public void testOpenMostRecentlyClosedEntry_NoWindowEntries() {
        when(mTabModel.getMostRecentClosureTime()).thenReturn(2L);
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(new ArrayList<>());

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);
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
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 3));
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(instanceInfoList);

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
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 2));
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(instanceInfoList);

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);
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
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 3));
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(instanceInfoList);

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
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 3));
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(instanceInfoList);

        mRecentlyClosedEntriesManager.openMostRecentlyClosedEntry(
                NewWindowAppSource.KEYBOARD_SHORTCUT);
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
    public void testOnWindowsClosed_NotPermanentDeletion_AddsWindow() {
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

        mRecentlyClosedEntriesManager.onWindowsClosed(
                Collections.singletonList(window), /* isPermanentDeletion= */ false);

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(2, entries.size());
        assertEquals(window, entries.get(0));
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowsClosed_PermanentDeletion_RemovesWindow_SingleWindow() {
        RecentlyClosedWindow window =
                new RecentlyClosedWindow(
                        /* timestamp= */ 10,
                        /* instanceId= */ 2,
                        /* url= */ "url",
                        /* title= */ "title",
                        /* activeTabTitle= */ "tab title",
                        /* tabCount= */ 1);
        mRecentlyClosedEntriesManager.onWindowsClosed(
                Collections.singletonList(window), /* isPermanentDeletion= */ false);
        assertEquals(1, mRecentlyClosedEntriesManager.getRecentlyClosedEntries().size());
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        mRecentlyClosedEntriesManager.onWindowsClosed(
                Collections.singletonList(window), /* isPermanentDeletion= */ true);

        assertEquals(0, mRecentlyClosedEntriesManager.getRecentlyClosedEntries().size());
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowsClosed_PermanentDeletion_RemovesWindow_MultipleWindows() {
        RecentlyClosedWindow window1 =
                new RecentlyClosedWindow(
                        /* timestamp= */ 10,
                        /* instanceId= */ 2,
                        /* url= */ "url",
                        /* title= */ "title",
                        /* activeTabTitle= */ "tab title",
                        /* tabCount= */ 1);
        RecentlyClosedWindow window2 =
                new RecentlyClosedWindow(
                        /* timestamp= */ 20,
                        /* instanceId= */ 3,
                        /* url= */ "url2",
                        /* title= */ "title2",
                        /* activeTabTitle= */ "tab title2",
                        /* tabCount= */ 2);

        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();
        mRecentlyClosedEntriesManager.onWindowsClosed(
                Arrays.asList(window1, window2), /* isPermanentDeletion= */ false);
        assertEquals(2, mRecentlyClosedEntriesManager.getRecentlyClosedEntries().size());
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());

        callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();
        mRecentlyClosedEntriesManager.onWindowsClosed(
                Arrays.asList(window1, window2), /* isPermanentDeletion= */ true);
        assertEquals(0, mRecentlyClosedEntriesManager.getRecentlyClosedEntries().size());
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowsClosed_MovesExistingWindowsToTop() {
        createRecentlyClosedWindows(/* numOfWindows= */ 3);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        RecentlyClosedWindow window1 = (RecentlyClosedWindow) entries.get(1);
        RecentlyClosedWindow window2 = (RecentlyClosedWindow) entries.get(2);
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        mRecentlyClosedEntriesManager.onWindowsClosed(
                Arrays.asList(window1, window2), /* isPermanentDeletion= */ false);

        entries = mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(3, entries.size());
        assertEquals(window1, entries.get(0));
        assertEquals(window2, entries.get(1));
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowsClosed_ExceedsMaxEntries_ClearWindowStorage() {
        int size = 25;
        createRecentlyClosedWindows(/* numOfWindows= */ 25);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();
        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();

        assertEquals(size, entries.size());
        RecentlyClosedWindow newWindow1 =
                new RecentlyClosedWindow(
                        /* timestamp= */ 10,
                        /* instanceId= */ 100,
                        /* url= */ "url",
                        /* title= */ "title",
                        /* activeTabTitle= */ "tab title",
                        /* tabCount= */ 1);
        RecentlyClosedWindow newWindow2 =
                new RecentlyClosedWindow(
                        /* timestamp= */ 20,
                        /* instanceId= */ 101,
                        /* url= */ "url2",
                        /* title= */ "title2",
                        /* activeTabTitle= */ "tab title2",
                        /* tabCount= */ 2);
        int callbackCount = mEntriesUpdatedCallbackHelper.getCallCount();

        // Verify the excess window entries is cleaned up from storage.
        mRecentlyClosedEntriesManager.onWindowsClosed(
                Arrays.asList(newWindow1, newWindow2), /* isPermanentDeletion= */ false);
        ArgumentCaptor<List<Integer>> listCaptor = ArgumentCaptor.forClass(List.class);
        verify(mMultiInstanceManager)
                .closeWindows(listCaptor.capture(), eq(CloseWindowAppSource.RECENT_TABS));
        assertEquals(2, listCaptor.getValue().size());
        verify(mRecentlyClosedTabManager, never()).clearLeastRecentlyUsedClosedEntries(anyInt());

        entries = mRecentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals(25, entries.size());
        assertEquals(newWindow1, entries.get(0));
        assertEquals(newWindow2, entries.get(1));
        assertEquals(callbackCount + 1, mEntriesUpdatedCallbackHelper.getCallCount());
    }

    @Test
    public void testOnWindowsClosed_ExceedsMaxEntries_ClearSessionEntryStorage() {
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
        mRecentlyClosedEntriesManager.onWindowsClosed(
                Collections.singletonList(newWindow), /* isPermanentDeletion= */ false);
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
        createRecentlyClosedWindows(/* numOfWindows= */ 1);
        List<RecentlyClosedEntry> sessionEntries = new ArrayList<>();
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 1, /* timestamp= */ 0));
        sessionEntries.add(
                new SessionRecentlyClosedEntry(
                        /* sessionId= */ 2,
                        /* timestamp= */ getDaysAgoMillis(/* numDaysAgo= */ 1) + 1));
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
        createRecentlyClosedWindows(/* numOfWindows= */ 1);
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
        createRecentlyClosedWindows(/* numOfWindows= */ 1);

        List<RecentlyClosedEntry> sessionEntries = new ArrayList<>();
        sessionEntries.add(new SessionRecentlyClosedEntry(/* sessionId= */ 1, /* timestamp= */ 0));
        sessionEntries.add(
                new SessionRecentlyClosedEntry(
                        /* sessionId= */ 2,
                        /* timestamp= */ getDaysAgoMillis(/* numDaysAgo= */ 1) + 3));

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
        sessionEntries.add(
                new SessionRecentlyClosedEntry(
                        /* sessionId= */ 1,
                        /* timestamp= */ getDaysAgoMillis(/* numDaysAgo= */ 1) + 3));
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

    @Test
    public void testUpdateRecentlyClosedEntries_InstanceInfoListIsNotSorted() {
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt())).thenReturn(null);
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ -1,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 2));
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 1,
                        /* taskId= */ -1,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 3));
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(instanceInfoList);

        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();

        assertEquals(2, entries.size());
        assertTrue(
                "Window entries are not sorted by most recent closure time.",
                entries.get(0).getDate().getTime() > entries.get(1).getDate().getTime());
    }

    @Test
    public void testUpdateRecentlyClosedEntries_InstanceClosureTimeUnavailable() {
        when(mRecentlyClosedTabManager.getRecentlyClosedEntries(anyInt())).thenReturn(null);
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ -1,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 2,
                        /* closureTime= */ 0));
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 1,
                        /* taskId= */ -1,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 3));
        instanceInfoList.add(
                new InstanceInfo(
                        /* instanceId= */ 2,
                        /* taskId= */ -1,
                        InstanceInfo.Type.OTHER,
                        /* url= */ "",
                        /* title= */ "",
                        /* customTitle= */ null,
                        /* tabCount= */ 0,
                        /* incognitoTabCount= */ 0,
                        /* isIncognitoSelected= */ false,
                        /* lastAccessedTime= */ 1,
                        /* closureTime= */ 1));
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(instanceInfoList);

        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        List<RecentlyClosedEntry> entries =
                mRecentlyClosedEntriesManager.getRecentlyClosedEntries();

        assertEquals(3, entries.size());
        assertEquals(3, entries.get(0).getDate().getTime());
        assertEquals(2, entries.get(1).getDate().getTime());
        assertEquals(1, entries.get(2).getDate().getTime());
    }

    @Test
    public void testGetRecentlyClosedWindowInternal() {
        // Set up test with a single closed window.
        createRecentlyClosedWindows(/* numOfWindows= */ 1);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Get the timestamp for the closed window entry.
        RecentlyClosedEntry entry = mRecentlyClosedEntriesManager.getRecentlyClosedEntries().get(0);
        RecentlyClosedWindow window = (RecentlyClosedWindow) entry;
        long timestamp = window.getDate().getTime();
        int instanceId = window.getInstanceId();

        // Mock out our dependencies. Always return mTabModelSelector.
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                new TabModelSelectorFactory() {
                    @Override
                    public TabModelSelector buildTabbedSelector(
                            Context context,
                            ModalDialogManager modalDialogManager,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier,
                            MultiInstanceManager multiInstanceManager) {
                        return mTabModelSelector;
                    }

                    @Override
                    public Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
                            @WindowId int windowId, Profile profile) {
                        return Pair.create(null, null);
                    }
                });
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);

        // Always return mTabWindowManager.
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.getTabModelSelectorById(anyInt())).thenReturn(mTabModelSelector);

        // Invoke the getRecentlyClosed() method with a callback.
        JniOnceCallback<RecentlyClosedWindowMetadata> callback = mock();
        mRecentlyClosedEntriesManager.getRecentlyClosedWindowInternal(
                TabWindowManager.INVALID_WINDOW_ID, callback);

        // Set up the expected callback result.
        RecentlyClosedWindowMetadata result = new RecentlyClosedWindowMetadata();
        result.tabModel = mTabModel;
        result.timestamp = timestamp;
        result.instanceId = instanceId;

        // The callback is invoked via task with the appropriate tab model.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    verify(callback).onResult(result);
                });
    }

    @Test
    public void testGetRecentlyClosedWindowInternal_UnknownInstanceId() {
        // Set up test with a single closed window.
        createRecentlyClosedWindows(/* numOfWindows= */ 1);
        mRecentlyClosedEntriesManager.updateRecentlyClosedEntries();

        // Make up a window instance ID that doesn't match this window.
        RecentlyClosedEntry entry = mRecentlyClosedEntriesManager.getRecentlyClosedEntries().get(0);
        RecentlyClosedWindow window = (RecentlyClosedWindow) entry;
        int badInstanceId = window.getInstanceId() + 1000;

        // Mock out our dependencies. Always return mTabModelSelector.
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                new TabModelSelectorFactory() {
                    @Override
                    public TabModelSelector buildTabbedSelector(
                            Context context,
                            ModalDialogManager modalDialogManager,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier,
                            MultiInstanceManager multiInstanceManager) {
                        return mTabModelSelector;
                    }

                    @Override
                    public Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
                            @WindowId int windowId, Profile profile) {
                        return Pair.create(null, null);
                    }
                });
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);

        // Always return mTabWindowManager.
        TabWindowManagerSingleton.setTabWindowManagerForTesting(mTabWindowManager);
        when(mTabWindowManager.getTabModelSelectorById(anyInt())).thenReturn(mTabModelSelector);

        // Invoke the getRecentlyClosed() method with the bad instance ID.
        JniOnceCallback<RecentlyClosedWindowMetadata> callback = mock();
        mRecentlyClosedEntriesManager.getRecentlyClosedWindowInternal(badInstanceId, callback);

        // The callback is invoked with null because no window with the instance ID was found.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    verify(callback).onResult(null);
                });
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
        long timestamp = getDaysAgoMillis(/* numDaysAgo= */ 1) + 1;
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
        long closureTime = getDaysAgoMillis(/* numDaysAgo= */ 1) + 2;
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
                            /* lastAccessedTime= */ closureTime - 1,
                            closureTime);
            instanceInfoList.add(0, instanceInfo);
            instanceId += 2;
            closureTime += 2;
        }
        when(mMultiInstanceManager.getRecentlyClosedInstances()).thenReturn(instanceInfoList);
    }

    private static long getDaysAgoMillis(int numDaysAgo) {
        return TimeUtils.currentTimeMillis() - TimeUnit.DAYS.toMillis(numDaysAgo);
    }
}
