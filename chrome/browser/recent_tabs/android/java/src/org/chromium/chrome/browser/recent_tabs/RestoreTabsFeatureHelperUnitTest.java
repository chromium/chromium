// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

/** Unit tests for RestoreTabsFeatureHelper. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsFeatureHelperUnitTest {
    private static final String RESTORE_TABS_FEATURE = FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock ForeignSessionHelper.Natives mForeignSessionHelperJniMock;
    @Mock private RestoreTabsControllerDelegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private Tracker mMockTracker;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Supplier<Integer> mGTSTabListModelSizeSupplier;
    @Mock private Callback<Integer> mScrollGTSToRestoredTabsCallback;
    private SharedPreferencesManager mSharedPreferencesManager;

    private Activity mActivity;
    private RestoreTabsFeatureHelper mHelper;

    @Before
    public void setUp() {
        ForeignSessionHelperJni.setInstanceForTesting(mForeignSessionHelperJniMock);
        TrackerFactory.setTrackerForTests(mMockTracker);
        when(mMockTracker.wouldTriggerHelpUi(eq(RESTORE_TABS_FEATURE))).thenReturn(true);
        when(mMockTracker.shouldTriggerHelpUi(eq(RESTORE_TABS_FEATURE))).thenReturn(true);
        when(mForeignSessionHelperJniMock.init(any(Profile.class))).thenReturn(1L);
        when(mForeignSessionHelperJniMock.isTabSyncEnabled(1L)).thenReturn(true);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mHelper = new RestoreTabsFeatureHelper();
        mHelper.setRestoreTabsControllerDelegateForTesting(mDelegate);

        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @Test
    public void testRestoreTabsFeatureHelper_noSyncedDevices() {
        mHelper.maybeShowPromo(
                mActivity,
                mProfile,
                mTabCreatorManager,
                mBottomSheetController,
                mGTSTabListModelSizeSupplier,
                mScrollGTSToRestoredTabsCallback,
                /* modalDialogManagerSupplier= */ null);
        verify(mForeignSessionHelperJniMock)
                .getMobileAndTabletForeignSessions(1L, new ArrayList<>());
        verify(mForeignSessionHelperJniMock).destroy(1L);
    }

    @Test
    public void testRestoreTabsFeatureHelper_tabSyncDisabled() {
        when(mForeignSessionHelperJniMock.isTabSyncEnabled(1L)).thenReturn(false);
        mHelper.maybeShowPromo(
                mActivity,
                mProfile,
                mTabCreatorManager,
                mBottomSheetController,
                mGTSTabListModelSizeSupplier,
                mScrollGTSToRestoredTabsCallback,
                /* modalDialogManagerSupplier= */ null);
        verify(mForeignSessionHelperJniMock, never())
                .getMobileAndTabletForeignSessions(anyLong(), any());
        verify(mForeignSessionHelperJniMock).destroy(1L);
    }

    @Test
    public void testRestoreTabsFeatureHelper_hasValidSyncedDevices() {
        // Setup mock data
        ForeignSessionTab tab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab);
        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);
        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, windows, FormFactor.PHONE);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(session);

        doAnswer(
                        invocation -> {
                            List<ForeignSession> invoked_sessions = invocation.getArgument(1);
                            invoked_sessions.addAll(sessions);
                            return true;
                        })
                .when(mForeignSessionHelperJniMock)
                .getMobileAndTabletForeignSessions(1L, new ArrayList<>());
        mHelper.maybeShowPromo(
                mActivity,
                mProfile,
                mTabCreatorManager,
                mBottomSheetController,
                mGTSTabListModelSizeSupplier,
                mScrollGTSToRestoredTabsCallback,
                /* modalDialogManagerSupplier= */ null);
        verify(mDelegate).showPromo(anyList());
    }

    @Test
    public void testRestoreTabsFeatureHelper_firstStartRecent_showsPromo() {
        long recentTimestamp = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, recentTimestamp);

        ForeignSessionTab tab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab);
        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);
        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, windows, FormFactor.PHONE);
        List<ForeignSession> sessionsToReturn = new ArrayList<>();
        sessionsToReturn.add(session);
        doAnswer(
                        invocation -> {
                            List<ForeignSession> invoked_sessions = invocation.getArgument(1);
                            invoked_sessions.addAll(sessionsToReturn);
                            return true;
                        })
                .when(mForeignSessionHelperJniMock)
                .getMobileAndTabletForeignSessions(eq(1L), eq(new ArrayList<>()));

        mHelper.maybeShowPromo(
                mActivity,
                mProfile,
                mTabCreatorManager,
                mBottomSheetController,
                mGTSTabListModelSizeSupplier,
                mScrollGTSToRestoredTabsCallback,
                /* modalDialogManagerSupplier= */ null);

        verify(mMockTracker).notifyEvent(eq(EventConstants.RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO));

        verify(mDelegate).showPromo(anyList());
        verify(mForeignSessionHelperJniMock, never()).destroy(eq(1L));
    }

    @Test
    public void testRestoreTabsFeatureHelper_firstStartTimestampNotSet_showsPromo() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP);

        ForeignSessionTab tab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab);
        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);
        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, windows, FormFactor.PHONE);
        List<ForeignSession> sessionsToReturn = new ArrayList<>();
        sessionsToReturn.add(session);
        doAnswer(
                        invocation -> {
                            List<ForeignSession> invoked_sessions = invocation.getArgument(1);
                            invoked_sessions.addAll(sessionsToReturn);
                            return true;
                        })
                .when(mForeignSessionHelperJniMock)
                .getMobileAndTabletForeignSessions(eq(1L), eq(new ArrayList<>()));

        mHelper.maybeShowPromo(
                mActivity,
                mProfile,
                mTabCreatorManager,
                mBottomSheetController,
                mGTSTabListModelSizeSupplier,
                mScrollGTSToRestoredTabsCallback,
                /* modalDialogManagerSupplier= */ null);

        verify(mMockTracker).notifyEvent(eq(EventConstants.RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO));

        verify(mDelegate).showPromo(anyList());
        verify(mForeignSessionHelperJniMock, never()).destroy(eq(1L));
    }

    @Test
    public void
            testRestoreTabsFeatureHelper_firstStartOld_doesNotShowPromoIfNotOtherwiseEligible() {
        long oldTimestamp = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(8);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, oldTimestamp);

        when(mMockTracker.wouldTriggerHelpUi(eq(RESTORE_TABS_FEATURE))).thenReturn(false);
        when(mMockTracker.shouldTriggerHelpUi(eq(RESTORE_TABS_FEATURE))).thenReturn(false);

        mHelper.maybeShowPromo(
                mActivity,
                mProfile,
                mTabCreatorManager,
                mBottomSheetController,
                mGTSTabListModelSizeSupplier,
                mScrollGTSToRestoredTabsCallback,
                /* modalDialogManagerSupplier= */ null);

        verify(mMockTracker, never())
                .notifyEvent(eq(EventConstants.RESTORE_TABS_ON_FIRST_RUN_SHOW_PROMO));

        verify(mDelegate, never()).showPromo(anyList());
        verify(mForeignSessionHelperJniMock, never()).init(any(Profile.class));
        verify(mForeignSessionHelperJniMock, never()).destroy(anyLong());
    }
}
