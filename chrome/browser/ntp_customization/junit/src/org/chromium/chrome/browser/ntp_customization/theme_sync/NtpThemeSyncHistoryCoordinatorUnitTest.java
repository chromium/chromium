// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.BackgroundCollection;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CollectionImage;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionManager;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataCustomizedColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataGroup;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataImageBase;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcher.Params;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.io.File;
import java.util.Arrays;
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

    private static final String TEST_COLLECTION_ID = "collection_id";
    private static final String TEST_COLLECTION_LABEL = "Label";
    private static final int TEST_COLLECTION_ORDER = 123;
    private static final String TEST_IMAGE_URL_1 = "https://img1.png/";
    private static final String TEST_IMAGE_URL_2 = "https://img2.png/";
    private static final String TEST_PREVIEW_URL_1 = "https://preview1.png/";
    private static final String TEST_PREVIEW_URL_2 = "https://preview2.png/";
    private static final String TEST_ATTRIBUTE_1 = "attr1";
    private static final String TEST_ATTRIBUTE_2 = "attr2";
    private static final int BITMAP_SIZE = 1;
    private static final int FULL_BITMAP_SIZE = 10;
    private static final String TEST_FILE_ID_HASH = "test_already_has_bitmap_hash";

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private View.OnClickListener mMoreOptionsClickListener;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    @Mock private NtpThemeCollectionManager mThemeCollectionManager;
    @Mock private Profile mProfile;
    @Captor private ArgumentCaptor<Callback<List<BackgroundCollection>>> mCollectionsCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<List<CollectionImage>>> mImagesCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mPreviewCallbackCaptor;

    private Context mContext;
    private NtpThemeSyncHistoryCoordinator mCoordinator;
    private NtpBackgroundDataManager mNtpBackgroundDataManager;
    private ViewGroup mParentView;
    private PropertyModel mPropertyModel;
    private ImageFetcher mMockImageFetcher;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);
        doAnswer(
                        invocation -> {
                            NtpBackgroundDataBase data = invocation.getArgument(1);
                            if (data instanceof NtpBackgroundDataThemeCollection themeData) {
                                if (!themeData.isBitmapSaved()) {
                                    Bitmap bitmap = themeData.getBitmap();
                                    if (bitmap != null) {
                                        NtpCustomizationUtils.saveBackgroundImageFile(
                                                themeData, bitmap);
                                    }
                                }
                            }
                            return null;
                        })
                .when(mNtpCustomizationConfigManager)
                .onBackgroundDataChanged(eq(mContext), any());

        mNtpBackgroundDataManager = new NtpBackgroundDataManager(mContext);
        mNtpBackgroundDataManager.resetSharedPreferenceForTesting();

        mParentView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.ntp_customization_main_bottom_sheet, null);

        mCoordinator =
                new NtpThemeSyncHistoryCoordinator(
                        mContext,
                        mParentView,
                        mBottomSheetDelegate,
                        mMoreOptionsClickListener,
                        mThemeCollectionManager,
                        mProfile);
        mPropertyModel = mCoordinator.getPropertyModelForTesting();
        mMockImageFetcher = mock(ImageFetcher.class);
        NtpCustomizationUtils.setImageFetcherForTesting(mMockImageFetcher);
        mBitmap = Bitmap.createBitmap(FULL_BITMAP_SIZE, FULL_BITMAP_SIZE, Bitmap.Config.ARGB_8888);
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
        assertEquals(3, dataList.size());
        // First three items should be default data options.
        assertTrue(dataList.get(0) instanceof NtpBackgroundDataColor);
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_ORANGE,
                ((NtpBackgroundDataColor) dataList.get(1)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_VIOLET,
                ((NtpBackgroundDataColor) dataList.get(2)).getThemeColorId());

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
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor);

        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localColor);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        assertEquals(4, dataList.size());
        // First item is Default 0, second is local history, third and fourth are other defaults.
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                ((NtpBackgroundDataColor) dataList.get(1)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_ORANGE,
                ((NtpBackgroundDataColor) dataList.get(2)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_VIOLET,
                ((NtpBackgroundDataColor) dataList.get(3)).getThemeColorId());

        // Highlighted index should be 1 (the local history item)
        assertEquals(
                1, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testPrepareToShow_WithLocalHistory_DefaultTheme() {
        // Save some local history.
        NtpBackgroundDataColor localColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor);

        // Current theme is default.
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(null);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        assertEquals(4, dataList.size());

        // Highlighted index should be 0 (the default item)
        assertEquals(
                0, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testPrepareToShow_WithLocalHistory_NonDefaultTheme() {
        // Save some local history.
        NtpBackgroundDataColor localColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);

        // Current theme is default.
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localColor);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        assertEquals(3, dataList.size());

        // Highlighted index should be NO_POSITION since localColor is not in local history
        assertEquals(
                RecyclerView.NO_POSITION,
                (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testPrepareToShow_WithLocalAndRemoteHistory() {
        // Save local history.
        NtpBackgroundDataCustomizedColor localColor =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        PlatformType.ANDROID,
                        /* primaryColorLight= */ Color.BLUE,
                        /* primaryColorDark= */ Color.BLUE,
                        /* ntpBackgroundColorLight= */ Color.WHITE,
                        /* ntpBackgroundColorDark= */ Color.BLACK);

        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor);

        // Save remote history (different from local).
        NtpBackgroundDataCustomizedColor remoteColor =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        PlatformType.IOS,
                        /* primaryColorLight= */ Color.CYAN,
                        /* primaryColorDark= */ Color.CYAN,
                        /* ntpBackgroundColorLight= */ Color.WHITE,
                        /* ntpBackgroundColorDark= */ Color.BLACK);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteColor);
        RobolectricUtil.runAllBackgroundAndUi();

        // Save another remote history which is duplicate of local.
        NtpBackgroundDataCustomizedColor remoteDuplicateColor =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        PlatformType.IOS,
                        /* primaryColorLight= */ Color.BLUE,
                        /* primaryColorDark= */ Color.BLUE,
                        /* ntpBackgroundColorLight= */ Color.WHITE,
                        /* ntpBackgroundColorDark= */ Color.BLACK);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteDuplicateColor);
        RobolectricUtil.runAllBackgroundAndUi();

        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localColor);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        // Should contain: Default, Local history (blue), Remote history (blue), Orange, Violet.
        assertEquals(5, dataList.size());
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(localColor, dataList.get(1));
        assertEquals(remoteDuplicateColor, dataList.get(2));
        assertEquals(
                NtpThemeColorId.NTP_COLORS_ORANGE,
                ((NtpBackgroundDataColor) dataList.get(3)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_VIOLET,
                ((NtpBackgroundDataColor) dataList.get(4)).getThemeColorId());

        // Highlighted index should be 1 (local history)
        assertEquals(
                1, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testOnItemClicked() {
        // Setup data: Default and one remote history (no local history)
        NtpBackgroundDataCustomizedColor remoteColor =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        PlatformType.IOS,
                        /* primaryColorLight= */ Color.BLUE,
                        /* primaryColorDark= */ Color.BLUE,
                        /* ntpBackgroundColorLight= */ Color.WHITE,
                        /* ntpBackgroundColorDark= */ Color.BLACK);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteColor);
        RobolectricUtil.runAllBackgroundAndUi();

        mCoordinator.prepareToShow();

        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(mCoordinator);

        int position = 1;
        // Click the remote history item (index 1, after Default)
        adapter.setSelectedPosition(position, /* isFromClick= */ true);

        assertBackgroundDataChangedImpl(remoteColor, /* expectedRecreate= */ true);

        // Click it again, should not trigger changes since it's already selected.
        clearInvocations(mNtpCustomizationConfigManager);
        adapter.setSelectedPosition(position, /* isFromClick= */ true);

        // Verify delegate isn't notified.
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
    }

    @Test
    public void testOnItemClicked_SelectOriginalItem() {
        // Setup data: Default and one local history.
        NtpBackgroundDataColor localColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localColor);

        mCoordinator.prepareToShow();

        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(mCoordinator);

        // Click the Default item (index 0), which is different from the original selected item
        // (index 1).
        int position = 0;
        adapter.setSelectedPosition(position, /* isFromClick= */ true);

        verify(mNtpCustomizationConfigManager).onBackgroundDataChanged(eq(mContext), any());
        verify(mBottomSheetDelegate).onNewColorSelected(eq(true));

        clearInvocations(mNtpCustomizationConfigManager, mBottomSheetDelegate);

        // Click back to the original selected item (index 1).
        position = 1;
        adapter.setSelectedPosition(position, /* isFromClick= */ true);

        assertBackgroundDataChangedImpl(localColor, /* expectedRecreate= */ false);
    }

    @Test
    public void testPrepareToShow_SubsequentCallUpdatesOnlyLocalHistory() {
        // 1. Initial Setup: one local (BLUE), one remote (CYAN).
        NtpBackgroundDataColor localColor1 =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor1);

        NtpBackgroundDataCustomizedColor remoteColor1 =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        PlatformType.IOS,
                        /* primaryColorLight= */ Color.CYAN,
                        /* primaryColorDark= */ Color.CYAN,
                        /* ntpBackgroundColorLight= */ Color.WHITE,
                        /* ntpBackgroundColorDark= */ Color.BLACK);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteColor1);
        RobolectricUtil.runAllBackgroundAndUi();

        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localColor1);

        // Call first time
        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        // Should contain: Default, localColor1 (blue), remoteColor1 (cyan), Orange, Violet.
        assertEquals(5, dataList.size());
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                ((NtpBackgroundDataColor) dataList.get(1)).getThemeColorId());
        assertEquals(remoteColor1, dataList.get(2));
        assertEquals(
                NtpThemeColorId.NTP_COLORS_ORANGE,
                ((NtpBackgroundDataColor) dataList.get(3)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_VIOLET,
                ((NtpBackgroundDataColor) dataList.get(4)).getThemeColorId());
        assertEquals(
                1, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));

        // 2. Update Setup: add new local (VIRIDIAN), add new remote (GREEN).
        NtpBackgroundDataColor localColor2 =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_VIRIDIAN,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor2);

        NtpBackgroundDataCustomizedColor remoteColor2 =
                new NtpBackgroundDataCustomizedColor(
                        mContext,
                        PlatformType.IOS,
                        /* primaryColorLight= */ Color.GREEN,
                        /* primaryColorDark= */ Color.GREEN,
                        /* ntpBackgroundColorLight= */ Color.WHITE,
                        /* ntpBackgroundColorDark= */ Color.BLACK);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteColor2);
        RobolectricUtil.runAllBackgroundAndUi();

        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localColor2);

        // Call second time.
        mCoordinator.prepareToShow();

        dataList = mCoordinator.getDataShowingListForTesting();
        // Should contain: Default, localColor2 (viridian), localColor1 (blue),
        // remoteColor1 (cyan), Orange, Violet.
        // remoteColor2 (green) should NOT be here because remote history is not reloaded.
        assertEquals(6, dataList.size());
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_VIRIDIAN,
                ((NtpBackgroundDataColor) dataList.get(1)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                ((NtpBackgroundDataColor) dataList.get(2)).getThemeColorId());
        assertEquals(remoteColor1, dataList.get(3));
        assertEquals(
                NtpThemeColorId.NTP_COLORS_ORANGE,
                ((NtpBackgroundDataColor) dataList.get(4)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_VIOLET,
                ((NtpBackgroundDataColor) dataList.get(5)).getThemeColorId());

        // Highlighted index should be 1 (localColor2, the new first local history item).
        assertEquals(
                1, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testPrepareToShow_WithLocalHistoryContainingDefaultOption() {
        // Save ORANGE (one of the extra default options) as local history.
        NtpBackgroundDataColor localColor =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_ORANGE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor);

        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localColor);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        // Should contain 3 items: DEFAULT, ORANGE (local history), and VIOLET (default option).
        assertEquals(3, dataList.size());
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_ORANGE,
                ((NtpBackgroundDataColor) dataList.get(1)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_VIOLET,
                ((NtpBackgroundDataColor) dataList.get(2)).getThemeColorId());

        // Highlighted index should be 1 (the local history item)
        assertEquals(
                1, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testPrepareToShow_WithMaxLocalHistory() {
        // Save 3 local history items (reaches MAXIMUM_LOCAL_HISTORY).
        NtpBackgroundDataColor localColor1 =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor1);
        NtpBackgroundDataColor localColor2 =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor2);
        NtpBackgroundDataColor localColor3 =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_GREEN,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localColor3);

        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localColor2);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        // Should contain 6 items: DEFAULT, the 3 local history items, and ORANGE, VIOLET to fill up
        // to MAXIMUM_HISTORY_ITEM.
        assertEquals(6, dataList.size());
        assertEquals(
                NtpThemeColorId.DEFAULT,
                ((NtpBackgroundDataColor) dataList.get(0)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_GREEN,
                ((NtpBackgroundDataColor) dataList.get(1)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_AQUA,
                ((NtpBackgroundDataColor) dataList.get(2)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                ((NtpBackgroundDataColor) dataList.get(3)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_ORANGE,
                ((NtpBackgroundDataColor) dataList.get(4)).getThemeColorId());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_VIOLET,
                ((NtpBackgroundDataColor) dataList.get(5)).getThemeColorId());

        // Highlighted index should be 2 (localColor2)
        assertEquals(
                2, (int) mPropertyModel.get(NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX));
    }

    @Test
    public void testInitDefaultOptions_WithThemeCollections() {
        BackgroundCollection collection =
                new BackgroundCollection(
                        TEST_COLLECTION_ID,
                        TEST_COLLECTION_LABEL,
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ORDER);
        CollectionImage image1 =
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        new GURL(TEST_IMAGE_URL_1),
                        new GURL(TEST_PREVIEW_URL_1),
                        Arrays.asList(TEST_ATTRIBUTE_1),
                        GURL.emptyGURL());
        CollectionImage image2 =
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        new GURL(TEST_IMAGE_URL_2),
                        new GURL(TEST_PREVIEW_URL_2),
                        Arrays.asList(TEST_ATTRIBUTE_2),
                        GURL.emptyGURL());

        clearInvocations(mMockImageFetcher);
        doAnswer(
                        invocation -> {
                            Callback<Bitmap> callback = invocation.getArgument(1);
                            callback.onResult(
                                    Bitmap.createBitmap(
                                            BITMAP_SIZE, BITMAP_SIZE, Bitmap.Config.ARGB_8888));
                            return null;
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any());

        NtpThemeCollectionManager mockManager = mock(NtpThemeCollectionManager.class);

        NtpThemeSyncHistoryCoordinator coordinator =
                new NtpThemeSyncHistoryCoordinator(
                        mContext,
                        mParentView,
                        mBottomSheetDelegate,
                        mMoreOptionsClickListener,
                        mockManager,
                        mProfile);

        verify(mockManager).getBackgroundCollections(mCollectionsCallbackCaptor.capture());
        mCollectionsCallbackCaptor.getValue().onResult(Arrays.asList(collection));

        verify(mockManager)
                .getBackgroundImages(eq(TEST_COLLECTION_ID), mImagesCallbackCaptor.capture());
        mImagesCallbackCaptor.getValue().onResult(Arrays.asList(image1, image2));

        verify(mMockImageFetcher, times(2)).fetchImage(any(), any());
        coordinator.prepareToShow();
        List<NtpBackgroundDataBase> dataList = coordinator.getDataShowingListForTesting();
        // It should contain: Default Color, Orange, Violet, 2 Theme Collections.
        assertEquals(5, dataList.size());
        assertTrue(dataList.get(0) instanceof NtpBackgroundDataColor);
        assertTrue(dataList.get(1) instanceof NtpBackgroundDataColor);
        assertTrue(dataList.get(2) instanceof NtpBackgroundDataColor);
        assertTrue(dataList.get(3) instanceof NtpBackgroundDataThemeCollection);
        assertTrue(dataList.get(4) instanceof NtpBackgroundDataThemeCollection);

        NtpBackgroundDataThemeCollection theme1 =
                (NtpBackgroundDataThemeCollection) dataList.get(3);
        assertEquals(TEST_COLLECTION_ID, theme1.getCustomBackgroundInfo().collectionId);
        assertEquals(TEST_IMAGE_URL_1, theme1.getCustomBackgroundInfo().backgroundUrl.getSpec());

        NtpBackgroundDataThemeCollection theme2 =
                (NtpBackgroundDataThemeCollection) dataList.get(4);
        assertEquals(TEST_COLLECTION_ID, theme2.getCustomBackgroundInfo().collectionId);
        assertEquals(TEST_IMAGE_URL_2, theme2.getCustomBackgroundInfo().backgroundUrl.getSpec());
    }

    @Test
    public void testPrepareToShow_ThemeCollectionsAvoidDuplicates() {
        CollectionImage image1 =
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        new GURL(TEST_IMAGE_URL_1),
                        new GURL(TEST_PREVIEW_URL_1),
                        Arrays.asList(TEST_ATTRIBUTE_1),
                        GURL.emptyGURL());

        // Save a theme collection to local history with the same URL.
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        new GURL(TEST_IMAGE_URL_1),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection localTheme =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        Bitmap.createBitmap(BITMAP_SIZE, BITMAP_SIZE, Bitmap.Config.ARGB_8888));
        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localTheme);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(localTheme);

        NtpThemeSyncHistoryCoordinator coordinator =
                setupThemeCollectionsAndCoordinator(new CollectionImage[] {image1});

        coordinator.prepareToShow();
        List<NtpBackgroundDataBase> dataList = coordinator.getDataShowingListForTesting();

        // Should contain 4 items: Default, Local History (theme1), Orange, Violet.
        // It should NOT contain theme1 again at the end!
        assertEquals(4, dataList.size());
        assertTrue(dataList.get(0) instanceof NtpBackgroundDataColor);
        assertEquals(localTheme, dataList.get(1));
        assertTrue(dataList.get(2) instanceof NtpBackgroundDataColor);
        assertTrue(dataList.get(3) instanceof NtpBackgroundDataColor);
    }

    @Test
    public void testOnItemClicked_ThemeCollectionFetchesImage() {
        CollectionImage image1 =
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        new GURL(TEST_IMAGE_URL_1),
                        new GURL(TEST_PREVIEW_URL_1),
                        Arrays.asList(TEST_ATTRIBUTE_1),
                        GURL.emptyGURL());

        NtpThemeSyncHistoryCoordinator coordinator =
                setupThemeCollectionsAndCoordinator(new CollectionImage[] {image1});

        coordinator.prepareToShow();
        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(coordinator);

        clearInvocations(mNtpCustomizationConfigManager, mBottomSheetDelegate, mMockImageFetcher);

        // Click the theme collection item (index 3).
        int position = 3;
        NtpBackgroundDataThemeCollection themeData =
                (NtpBackgroundDataThemeCollection)
                        coordinator.getDataShowingListForTesting().get(position);
        assertNull(themeData.getBitmap());

        // Setup image fetcher for the full image click.
        Bitmap fullBitmap =
                Bitmap.createBitmap(FULL_BITMAP_SIZE, FULL_BITMAP_SIZE, Bitmap.Config.ARGB_8888);
        doAnswer(
                        invocation -> {
                            Callback<Bitmap> callback = invocation.getArgument(1);
                            callback.onResult(fullBitmap);
                            return null;
                        })
                .when(mMockImageFetcher)
                .fetchImage(any(), any());

        adapter.setSelectedPosition(position, /* isFromClick= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify it fetches the image again.
        ArgumentCaptor<Params> paramsCaptor = ArgumentCaptor.forClass(Params.class);
        verify(mMockImageFetcher).fetchImage(paramsCaptor.capture(), any());
        assertEquals(TEST_IMAGE_URL_1, paramsCaptor.getValue().url);

        // Verify the bitmap is set.
        assertEquals(fullBitmap, themeData.getBitmap());
        // Verify primary color and other fields are set.
        assertNotNull(themeData.getPrimaryColor());
        assertNotNull(themeData.getFileIdHash());
        assertNotNull(themeData.getBackgroundImageInfo());

        assertBackgroundDataChangedImpl(themeData, /* expectedRecreate= */ true);

        // Verify the file is saved to disk and isBitmapSaved is true.
        assertTrue(themeData.isBitmapSaved());
        File expectedSavedFile = new File(themeData.getLastUploadImageFilePath());
        assertTrue(expectedSavedFile.exists());
    }

    @Test
    public void testOnItemClicked_ThemeCollectionAlreadyHasBitmap() {
        CollectionImage image1 =
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        new GURL(TEST_IMAGE_URL_1),
                        new GURL(TEST_PREVIEW_URL_1),
                        Arrays.asList(TEST_ATTRIBUTE_1),
                        GURL.emptyGURL());

        NtpThemeSyncHistoryCoordinator coordinator =
                setupThemeCollectionsAndCoordinator(new CollectionImage[] {image1});

        coordinator.prepareToShow();
        NtpBackgroundDataThemeCollection themeData =
                (NtpBackgroundDataThemeCollection)
                        coordinator.getDataShowingListForTesting().get(3);

        // Manually simulate that it already has the bitmap, has been saved, and has a fileIdHash.
        Bitmap fullBitmap =
                Bitmap.createBitmap(FULL_BITMAP_SIZE, FULL_BITMAP_SIZE, Bitmap.Config.ARGB_8888);
        themeData.setBitmap(fullBitmap);
        themeData.setFileIdHash(TEST_FILE_ID_HASH);
        themeData.setIsBitmapSaved(/* isBitmapSaved= */ true);

        // Pre-create the file and then delete it to verify it won't be saved again.
        File expectedSavedFile = new File(themeData.getLastUploadImageFilePath());
        NtpCustomizationUtils.saveBackgroundImageFile(themeData, fullBitmap);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(expectedSavedFile.exists());
        expectedSavedFile.delete();
        assertFalse(expectedSavedFile.exists());

        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(coordinator);

        clearInvocations(mNtpCustomizationConfigManager, mBottomSheetDelegate, mMockImageFetcher);

        // Click the theme collection item (index 3).
        int position = 3;
        adapter.setSelectedPosition(position, /* isFromClick= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify it does NOT fetch the image again.
        verify(mMockImageFetcher, never()).fetchImage(any(), any());

        assertBackgroundDataChangedImpl(themeData, /* expectedRecreate= */ true);

        // Verify the file was NOT saved again (does not exist).
        assertFalse(expectedSavedFile.exists());
    }

    @Test
    public void testOnThemeCollectionPreviewBitmapAvailable_CallsNotifyItemInserted() {
        CollectionImage image1 =
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        new GURL(TEST_IMAGE_URL_1),
                        new GURL(TEST_PREVIEW_URL_1),
                        Arrays.asList(TEST_ATTRIBUTE_1),
                        GURL.emptyGURL());

        NtpThemeSyncHistoryCoordinator coordinator =
                setupThemeCollectionsAndCoordinatorInternal(
                        /* fetchPreviewsSynchronously= */ false, new CollectionImage[] {image1});

        // Verify fetchImage was called, and capture the callback.
        verify(mMockImageFetcher).fetchImage(any(), mPreviewCallbackCaptor.capture());

        // Now call prepareToShow.
        coordinator.prepareToShow();

        // Register an observer to verify the adapter notification.
        androidx.recyclerview.widget.RecyclerView.AdapterDataObserver mockObserver =
                mock(androidx.recyclerview.widget.RecyclerView.AdapterDataObserver.class);
        coordinator.getRecyclerViewAdaptorForTesting().registerAdapterDataObserver(mockObserver);

        // Simulate the preview bitmap arriving.
        Bitmap previewBitmap =
                Bitmap.createBitmap(BITMAP_SIZE, BITMAP_SIZE, Bitmap.Config.ARGB_8888);
        mPreviewCallbackCaptor.getValue().onResult(previewBitmap);

        // Verify that onItemRangeInserted was called for the new item!
        int expectedIndex = coordinator.getDataShowingListForTesting().size() - 1;
        verify(mockObserver).onItemRangeInserted(eq(expectedIndex), eq(1));
    }

    @Test
    public void testOnItemClicked_LocalHistoryAlreadyHasBitmapDoesNotSaveToDisk() {

        String fileIdHash = "test_hash_saves";
        NtpBackgroundDataImageBase localThemeInList =
                prepareLocalHistoryWithThemeCollectionImpl(fileIdHash);

        String filePath = localThemeInList.getLastUploadImageFilePath();
        Bitmap diskBitmap =
                Bitmap.createBitmap(FULL_BITMAP_SIZE, FULL_BITMAP_SIZE, Bitmap.Config.ARGB_8888);
        NtpCustomizationUtils.saveBackgroundImageFile(localThemeInList, diskBitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        localThemeInList.getBitmapOrLoadImage((result) -> {});
        RobolectricUtil.runAllBackgroundAndUi();
        assertNotNull(localThemeInList.getBitmap());

        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(mCoordinator);

        // Pre-create the file and then delete it to verify it won't be saved again.
        File expectedSavedFile = new File(filePath);
        assertTrue(expectedSavedFile.exists());
        expectedSavedFile.delete();
        assertFalse(expectedSavedFile.exists());

        clearInvocations(mNtpCustomizationConfigManager, mBottomSheetDelegate, mMockImageFetcher);
        int position = 1;
        adapter.setSelectedPosition(position, /* isFromClick= */ true);

        RobolectricUtil.runAllBackgroundAndUi();

        // Verify the file was NOT saved again, and it applies the theme directly without fetching.
        assertFalse(expectedSavedFile.exists());
        verify(mMockImageFetcher, never()).fetchImage(any(), any());
        assertBackgroundDataChangedImpl(localThemeInList, /* expectedRecreate= */ true);
    }

    @Test
    public void testOnItemClicked_LocalHistoryNullBitmapDoesNotSave() {

        String fileIdHash = "test_hash_null_bitmap";
        NtpBackgroundDataImageBase localThemeInList =
                prepareLocalHistoryWithThemeCollectionImpl(fileIdHash);

        String filePath = localThemeInList.getLastUploadImageFilePath();
        File expectedSavedFile = new File(filePath);
        if (expectedSavedFile.exists()) {
            expectedSavedFile.delete();
        }
        assertFalse(expectedSavedFile.exists());

        assertNull(localThemeInList.getBitmap());

        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(mCoordinator);

        clearInvocations(mMockImageFetcher);
        int position = 1;
        adapter.setSelectedPosition(position, /* isFromClick= */ true);

        RobolectricUtil.runAllBackgroundAndUi();

        assertFalse(expectedSavedFile.exists());
        verify(mMockImageFetcher).fetchImage(any(), any());
    }

    private NtpBackgroundDataImageBase prepareLocalHistoryWithThemeCollectionImpl(
            String fileIdHash) {
        BackgroundCollection collection =
                new BackgroundCollection(
                        TEST_COLLECTION_ID,
                        TEST_COLLECTION_LABEL,
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ORDER);
        CollectionImage image2 =
                new CollectionImage(
                        TEST_COLLECTION_ID,
                        new GURL(TEST_IMAGE_URL_2),
                        new GURL(TEST_PREVIEW_URL_2),
                        Arrays.asList(TEST_ATTRIBUTE_1),
                        GURL.emptyGURL());

        verify(mThemeCollectionManager)
                .getBackgroundCollections(mCollectionsCallbackCaptor.capture());
        mCollectionsCallbackCaptor.getValue().onResult(Arrays.asList(collection));

        verify(mThemeCollectionManager)
                .getBackgroundImages(eq(TEST_COLLECTION_ID), mImagesCallbackCaptor.capture());
        mImagesCallbackCaptor.getValue().onResult(Arrays.asList(image2));

        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        new GURL(TEST_IMAGE_URL_1),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection localTheme =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        fileIdHash);

        mNtpBackgroundDataManager.saveUserSelectedBackgroundTypeToSharedPreference(localTheme);

        mCoordinator.prepareToShow();

        List<NtpBackgroundDataBase> list = mCoordinator.getDataShowingListForTesting();
        return (NtpBackgroundDataImageBase) list.get(1);
    }

    private NtpThemeSyncHistoryCoordinator setupThemeCollectionsAndCoordinator(
            CollectionImage[] images) {
        NtpThemeSyncHistoryCoordinator coordinator =
                setupThemeCollectionsAndCoordinatorInternal(
                        /* fetchPreviewsSynchronously= */ true, images);
        verify(mMockImageFetcher, times(images.length)).fetchImage(any(), any());
        return coordinator;
    }

    private NtpThemeSyncHistoryCoordinator setupThemeCollectionsAndCoordinatorInternal(
            boolean fetchPreviewsSynchronously, CollectionImage[] images) {
        BackgroundCollection collection =
                new BackgroundCollection(
                        TEST_COLLECTION_ID,
                        TEST_COLLECTION_LABEL,
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ORDER);

        clearInvocations(mMockImageFetcher);
        if (fetchPreviewsSynchronously) {
            doAnswer(
                            invocation -> {
                                Callback<Bitmap> callback = invocation.getArgument(1);
                                callback.onResult(
                                        Bitmap.createBitmap(
                                                BITMAP_SIZE, BITMAP_SIZE, Bitmap.Config.ARGB_8888));
                                return null;
                            })
                    .when(mMockImageFetcher)
                    .fetchImage(any(), any());
        }

        NtpThemeCollectionManager mockManager = mock(NtpThemeCollectionManager.class);

        NtpThemeSyncHistoryCoordinator coordinator =
                new NtpThemeSyncHistoryCoordinator(
                        mContext,
                        mParentView,
                        mBottomSheetDelegate,
                        mMoreOptionsClickListener,
                        mockManager,
                        mProfile);

        verify(mockManager).getBackgroundCollections(mCollectionsCallbackCaptor.capture());
        mCollectionsCallbackCaptor.getValue().onResult(Arrays.asList(collection));

        verify(mockManager)
                .getBackgroundImages(eq(TEST_COLLECTION_ID), mImagesCallbackCaptor.capture());
        mImagesCallbackCaptor.getValue().onResult(Arrays.asList(images));

        return coordinator;
    }

    private void assertBackgroundDataChangedImpl(
            NtpBackgroundDataBase expectedData, boolean expectedRecreate) {
        verify(mNtpCustomizationConfigManager)
                .onBackgroundDataChanged(eq(mContext), eq(expectedData));
        verify(mBottomSheetDelegate).onNewColorSelected(eq(expectedRecreate));
    }

    @Test
    public void testPrepareToShow_WithRemoteThemeCollection_FetchesImageAndUpdates() {
        NtpBackgroundDataThemeCollection remoteTheme =
                createRemoteThemeCollectionImpl(/* isBitmapSaved= */ false);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteTheme);
        RobolectricUtil.runAllBackgroundAndUi();

        clearInvocations(mMockImageFetcher, mNtpCustomizationConfigManager);
        mCoordinator.prepareToShow();

        ArgumentCaptor<Params> paramsCaptor = ArgumentCaptor.forClass(Params.class);
        verify(mMockImageFetcher)
                .fetchImage(paramsCaptor.capture(), mPreviewCallbackCaptor.capture());
        assertEquals(TEST_IMAGE_URL_1, paramsCaptor.getValue().url);

        mPreviewCallbackCaptor.getValue().onResult(mBitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        int position = findRemoteThemeCollectionPositionImpl(dataList);
        assertTrue(position != -1);

        NtpBackgroundDataThemeCollection updatedTheme =
                (NtpBackgroundDataThemeCollection) dataList.get(position);
        assertEquals(mBitmap, updatedTheme.getBitmap());
        assertNotNull(updatedTheme.getBackgroundImageInfo());

        NtpBackgroundDataGroup remoteGroup =
                mNtpBackgroundDataManager.getBackgroundDataGroupFromSharedPreference(
                        PlatformType.IOS);
        assertEquals(1, remoteGroup.size());
        NtpBackgroundDataThemeCollection savedTheme =
                (NtpBackgroundDataThemeCollection) remoteGroup.get(0);
        assertNotNull(savedTheme.getBackgroundImageInfo());
        assertFalse(savedTheme.isBitmapSaved());
    }

    @Test
    public void
            testOnItemClicked_RemoteThemeCollectionFirstClick_SavesToDiskAndUpdatesRemoteSyncData() {
        NtpBackgroundDataThemeCollection remoteTheme =
                createRemoteThemeCollectionImpl(/* isBitmapSaved= */ false);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteTheme);
        RobolectricUtil.runAllBackgroundAndUi();

        mCoordinator.prepareToShow();
        ArgumentCaptor<Params> paramsCaptor = ArgumentCaptor.forClass(Params.class);
        verify(mMockImageFetcher)
                .fetchImage(paramsCaptor.capture(), mPreviewCallbackCaptor.capture());

        mPreviewCallbackCaptor.getValue().onResult(mBitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(mCoordinator);
        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        int position = findRemoteThemeCollectionPositionImpl(dataList);
        assertTrue(position != -1);

        NtpBackgroundDataThemeCollection inMemoryTheme =
                (NtpBackgroundDataThemeCollection) dataList.get(position);
        assertNotNull(inMemoryTheme.getBitmap());
        assertFalse(inMemoryTheme.isBitmapSaved());

        clearInvocations(mNtpCustomizationConfigManager, mBottomSheetDelegate, mMockImageFetcher);

        adapter.setSelectedPosition(position, /* isFromClick= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockImageFetcher, never()).fetchImage(any(), any());
        assertTrue(inMemoryTheme.isBitmapSaved());
        assertBackgroundDataChangedImpl(inMemoryTheme, /* expectedRecreate= */ true);

        NtpBackgroundDataGroup remoteGroup =
                mNtpBackgroundDataManager.getBackgroundDataGroupFromSharedPreference(
                        PlatformType.IOS);
        assertEquals(1, remoteGroup.size());
        NtpBackgroundDataThemeCollection savedTheme =
                (NtpBackgroundDataThemeCollection) remoteGroup.get(0);
        assertTrue(savedTheme.isBitmapSaved());
    }

    @Test
    public void testOnItemClicked_RemoteThemeCollectionSubsequentClick_DoesNotFetchAgain() {
        NtpBackgroundDataThemeCollection remoteTheme =
                createRemoteThemeCollectionImpl(/* isBitmapSaved= */ true);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteTheme);
        RobolectricUtil.runAllBackgroundAndUi();

        mCoordinator.prepareToShow();
        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(mCoordinator);

        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        int position = findRemoteThemeCollectionPositionImpl(dataList);
        assertTrue(position != -1);

        // Simulate that the bitmap has already been fetched and set in memory in this session.
        NtpBackgroundDataThemeCollection inMemoryTheme =
                (NtpBackgroundDataThemeCollection) dataList.get(position);
        inMemoryTheme.setBitmap(mBitmap);
        assertTrue(inMemoryTheme.isBitmapSaved());

        clearInvocations(mNtpCustomizationConfigManager, mBottomSheetDelegate, mMockImageFetcher);
        adapter.setSelectedPosition(position, /* isFromClick= */ true);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mMockImageFetcher, never()).fetchImage(any(), any());
        assertBackgroundDataChangedImpl(inMemoryTheme, /* expectedRecreate= */ true);
    }

    @Test
    public void
            testOnItemClicked_RemoteThemeCollectionSubsequentClick_NoBitmapSet_FetchesBitmapAndApplies() {
        NtpBackgroundDataThemeCollection remoteTheme =
                createRemoteThemeCollectionImpl(/* isBitmapSaved= */ true);
        mNtpBackgroundDataManager.saveRemoteSyncDataToSharedPreference(remoteTheme);
        RobolectricUtil.runAllBackgroundAndUi();

        clearInvocations(mMockImageFetcher);
        mCoordinator.prepareToShow();

        verify(mMockImageFetcher, never()).fetchImage(any(), any());

        NtpThemeSyncHistoryRecyclerViewAdaptor adapter = getAdapterImpl(mCoordinator);
        List<NtpBackgroundDataBase> dataList = mCoordinator.getDataShowingListForTesting();
        int position = findRemoteThemeCollectionPositionImpl(dataList);
        assertTrue(position != -1);

        NtpBackgroundDataThemeCollection inMemoryTheme =
                (NtpBackgroundDataThemeCollection) dataList.get(position);
        assertNull(inMemoryTheme.getBitmap());

        clearInvocations(mNtpCustomizationConfigManager, mBottomSheetDelegate, mMockImageFetcher);

        adapter.setSelectedPosition(position, /* isFromClick= */ true);

        ArgumentCaptor<Params> paramsCaptor = ArgumentCaptor.forClass(Params.class);
        verify(mMockImageFetcher)
                .fetchImage(paramsCaptor.capture(), mPreviewCallbackCaptor.capture());
        assertEquals(TEST_IMAGE_URL_1, paramsCaptor.getValue().url);

        mPreviewCallbackCaptor.getValue().onResult(mBitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(mBitmap, inMemoryTheme.getBitmap());
        assertNotNull(inMemoryTheme.getPrimaryColor());
        assertBackgroundDataChangedImpl(inMemoryTheme, /* expectedRecreate= */ true);
    }

    private NtpBackgroundDataThemeCollection createRemoteThemeCollectionImpl(
            boolean isBitmapSaved) {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        new GURL(TEST_IMAGE_URL_1),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection remoteTheme =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.IOS,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        "remote_theme_hash");
        remoteTheme.setIsBitmapSaved(isBitmapSaved);
        return remoteTheme;
    }

    private int findRemoteThemeCollectionPositionImpl(List<NtpBackgroundDataBase> dataList) {
        for (int i = 0; i < dataList.size(); i++) {
            NtpBackgroundDataBase data = dataList.get(i);
            if (data instanceof NtpBackgroundDataThemeCollection theme
                    && theme.getPlatformType() == PlatformType.IOS) {
                return i;
            }
        }
        return -1;
    }

    private NtpThemeSyncHistoryRecyclerViewAdaptor getAdapterImpl(
            NtpThemeSyncHistoryCoordinator coordinator) {
        NtpThemeSyncHistoryRecyclerViewAdaptor adapter =
                coordinator.getRecyclerViewAdaptorForTesting();
        assertNotNull(adapter);
        return adapter;
    }
}
