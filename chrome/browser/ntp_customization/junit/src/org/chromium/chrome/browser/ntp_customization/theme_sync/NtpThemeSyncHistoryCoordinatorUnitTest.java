// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link NtpThemeSyncHistoryCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({
    ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
    ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
})
public class NtpThemeSyncHistoryCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private View.OnClickListener mMoreOptionsClickListener;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;

    private Context mContext;
    private NtpThemeSyncHistoryCoordinator mCoordinator;
    private NtpBackgroundDataManager mNtpBackgroundDataManager;
    private ViewGroup mParentView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);

        mNtpBackgroundDataManager = new NtpBackgroundDataManager(mContext);
        mNtpBackgroundDataManager.resetSharedPreferenceForTesting();

        mParentView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.ntp_customization_main_bottom_sheet, null);

        mCoordinator =
                new NtpThemeSyncHistoryCoordinator(
                        mContext, mParentView, mBottomSheetDelegate, mMoreOptionsClickListener);
        mPropertyModel = mCoordinator.getPropertyModelForTesting();
    }

    @After
    public void tearDown() {
        mNtpBackgroundDataManager.resetSharedPreferenceForTesting();
        NtpCustomizationConfigManager.getInstance().resetForTesting();
        if (mCoordinator != null) {
            mCoordinator.destroy();
        }
    }

    @Test
    public void testConstructor() {
        assertTrue(mPropertyModel.get(NtpThemeSyncHistoryProperties.IS_VISIBLE));
        assertNotNull(
                mPropertyModel.get(NtpThemeSyncHistoryProperties.RECYCLER_VIEW_LAYOUT_MANAGER));
        assertEquals(
                mMoreOptionsClickListener,
                mPropertyModel.get(NtpThemeSyncHistoryProperties.MORE_OPTIONS_CLICK_LISTENER));
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();
        assertNull(mPropertyModel.get(NtpThemeSyncHistoryProperties.MORE_OPTIONS_CLICK_LISTENER));
        assertNull(mPropertyModel.get(NtpThemeSyncHistoryProperties.RECYCLER_VIEW_LAYOUT_MANAGER));
    }

    @Test
    public void testPrepareToShow_NoHistory() {
        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        assertEquals(1, dataList.size());
        // First item should be default data.
        assertTrue(dataList.get(0) instanceof NtpBackgroundDataColor);
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());

        assertEquals(
                0, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
        assertNotNull(mPropertyModel.get(NtpThemeSyncHistoryProperties.RECYCLER_VIEW_ADAPTER));
    }

    @Test
    public void testPrepareToShow_WithLocalHistory() {
        // Save some local history.
        NtpBackgroundDataColor localColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID_LOCAL,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        assertEquals(2, dataList.size());
        // First item is default, second is local history
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                ((NtpBackgroundDataColor) dataList.get(1)).getThemeColorId());

        // Highlighted index should be 1 (the local history item)
        assertEquals(
                1, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testPrepareToShow_WithLocalAndRemoteHistory() {
        // Save local history.
        NtpBackgroundDataColor localColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID_LOCAL,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor);

        // Save remote history (different from local).
        NtpBackgroundDataColor remoteColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID_REMOTE,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteColor);

        // Save another remote history which is duplicate of local.
        NtpBackgroundDataColor remoteDuplicateColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID_REMOTE,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteDuplicateColor);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        // Should contain: Default (local), Local history (blue), Remote history (blue).
        assertEquals(3, dataList.size());
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                ((NtpBackgroundDataColor) dataList.get(1)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                ((NtpBackgroundDataColor) dataList.get(2)).getThemeColorId());

        // Highlighted index should be 1 (local history)
        assertEquals(
                1, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testOnItemClicked() {
        // Setup data: Default and one remote history (no local history)
        NtpBackgroundDataColor remoteColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID_REMOTE,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteColor);

        mCoordinator.prepareToShow();

        NtpThemeSyncHistoryRecyclerViewAdaptor adapter =
                mCoordinator.getRecyclerViewAdaptorForTesting();
        assertNotNull(adapter);

        int position = 1;
        boolean isFromClick = true;
        // Click the remote history item (index 1)
        adapter.setSelectedPosition(position, isFromClick);

        // Verify config manager is notified.
        verify(mNtpCustomizationConfigManager)
                .onBackgroundDataChanged(eq(mContext), eq(remoteColor));
        // Verify delegate is notified (it is a different color from default, so true)
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));

        // Click it again, should not trigger changes since it's already selected.
        clearInvocations(mNtpCustomizationConfigManager);
        adapter.setSelectedPosition(position, isFromClick);

        // Verify delegate isn't notified.
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
    }
}
