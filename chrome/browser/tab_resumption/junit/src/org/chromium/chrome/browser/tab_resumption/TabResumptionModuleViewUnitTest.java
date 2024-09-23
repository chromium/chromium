// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ClickInfo;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tab_resumption.UrlImageProvider.UrlImageCallback;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
public class TabResumptionModuleViewUnitTest extends TestSupportExtended {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mocker = new JniMocker();

    @Mock UrlUtilities.Natives mUrlUtilitiesJniMock;

    private static final String TAB_TITLE = "Tab Title";
    private static final String REASON_TO_SHOW_TAB = "Your most recent Tab";
    private static final int TAB_ID = 11;
    private static final int TRACKING_TAB_ID = 1;

    @Mock private UrlImageProvider mUrlImageProvider;
    @Mock private Tab mTab;
    @Mock private Tab mTrackingTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Callback<Tab> mTabObserverCallback;
    @Mock private Callback<Integer> mOnModuleShowConfigFinalizedCallback;

    @Captor private ArgumentCaptor<GURL> mFetchImagePageUrlCaptor;
    @Captor private ArgumentCaptor<Callback<Drawable>> mThumbnailCallbackCaptor;
    @Captor private ArgumentCaptor<UrlImageCallback> mFetchImageCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mFetchSalientImageCallbackCaptor;
    @Captor private ArgumentCaptor<TabModelSelectorObserver> mTabModelSelectorObserverCaptor;

    private TabResumptionModuleView mModuleView;
    private Size mThumbnailSize;
    private TabResumptionTileContainerView mTileContainerView;

    private SuggestionClickCallback mClickCallback;
    private SuggestionBundle mSuggestionBundle;

    private SuggestionEntry mLastClickEntry;
    private int mClickCount;
    private Context mContext;
    private ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier;
    private List<Tab> mTabsInTabModel = new ArrayList<>();

    @Before
    public void setUp() {
        mocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);

        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mTab.getTitle()).thenReturn(TAB_TITLE);
        when(mTab.getTimestampMillis()).thenReturn(makeTimestamp(24 - 3, 0, 0));
        when(mTab.getId()).thenReturn(TAB_ID);

        when(mTrackingTab.getUrl()).thenReturn(JUnitTestGURLs.BLUE_1);
        when(mTrackingTab.getTitle()).thenReturn(TAB_TITLE);
        when(mTrackingTab.getTimestampMillis()).thenReturn(makeTimestamp(24 - 3, 0, 0));
        when(mTrackingTab.getId()).thenReturn(TRACKING_TAB_ID);

        int size =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.single_tab_module_tab_thumbnail_size_big);
        mThumbnailSize = new Size(size, size);

        mClickCallback =
                (SuggestionEntry entry) -> {
                    mLastClickEntry = entry;
                    ++mClickCount;
                };
        mSuggestionBundle = new SuggestionBundle(CURRENT_TIME_MS);

        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        mTabModelSelectorSupplier = new ObservableSupplierImpl<>();
        mTabModelSelectorSupplier.set(mTabModelSelector);
    }

    @After
    public void tearDown() {
        mTabsInTabModel.clear();
        mModuleView.destroy();
        assertTrue(mTileContainerView.isCallbackControllerNullForTesting());
        mModuleView = null;
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        initModuleView();

        String testTitle1 = "This is a test title";
        String testTitle2 = "Here is another test title";
        TextView titleTextView = mModuleView.findViewById(R.id.tab_resumption_title_description);

        mModuleView.setTitle(testTitle1);
        Assert.assertEquals(testTitle1, titleTextView.getText());
        mModuleView.setTitle(testTitle2);
        Assert.assertEquals(testTitle2, titleTextView.getText());
        mModuleView.setTitle(null);
        Assert.assertEquals("", titleTextView.getText());
    }

    @Test
    @SmallTest
    public void testRenderSingle() {
        initModuleView();

        SuggestionEntry entry1 =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0));
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Check tile texts.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        assertTrue(
                TextUtils.isEmpty(
                        ((TextView) tile1.findViewById(R.id.tile_pre_info_text)).getText()));
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.google.com \u2022 Desktop",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertNull(mLastClickEntry);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickEntry.url);
    }

    @Test
    @SmallTest
    public void testRenderSingle_SalientImage() {
        TabResumptionModuleUtils.TAB_RESUMPTION_USE_SALIENT_IMAGE.setForTesting(true);
        initModuleView();

        SuggestionEntry entry1 =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0));
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchSalientImage(
                        mFetchImagePageUrlCaptor.capture(),
                        eq(true),
                        mFetchSalientImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Check tile texts.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        assertTrue(
                TextUtils.isEmpty(
                        ((TextView) tile1.findViewById(R.id.tile_pre_info_text)).getText()));
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.google.com \u2022 Desktop",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchSalientImageCallbackCaptor.getAllValues().get(0).onResult(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertNull(mLastClickEntry);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickEntry.url);
    }

    @Test
    @SmallTest
    public void testLoadTileUrlImageWithSalientImage() {
        TabResumptionModuleUtils.TAB_RESUMPTION_USE_SALIENT_IMAGE.setForTesting(true);
        initModuleView();

        String histogramName = "MagicStack.Clank.TabResumption.IsSalientImageAvailable";
        GURL expectedUrl = JUnitTestGURLs.BLUE_3;
        SuggestionEntry entry1 =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "My Tablet",
                        /* url= */ expectedUrl,
                        /* title= */ "Blue website with a very long title that might not fit",
                        /* timestamp= */ makeTimestamp(24 - 1, 60 - 16, 0));
        TabResumptionTileView tile1 = Mockito.mock(TabResumptionTileView.class);

        mTileContainerView.loadTileUrlImage(
                entry1,
                mUrlImageProvider,
                tile1,
                /* isSingle= */ false,
                /* usSalientImage= */ true);

        verify(mUrlImageProvider)
                .fetchSalientImage(
                        eq(expectedUrl),
                        /* isSingle= */ eq(false),
                        mFetchSalientImageCallbackCaptor.capture());

        // Verifies the case that a salient image is returned.
        Bitmap bitmap = makeBitmap(100, 100);
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectBooleanRecord(histogramName, true).build();
        mFetchSalientImageCallbackCaptor.getValue().onResult(bitmap);
        verify(tile1).setImageDrawable(any(Drawable.class));
        verify(tile1).updateForSalientImage();
        histogramWatcher.assertExpected();

        // Verifies the case that no salient image is available.
        mFetchSalientImageCallbackCaptor.getValue().onResult(null);
        verify(mUrlImageProvider)
                .fetchImageForUrl(eq(expectedUrl), mFetchImageCallbackCaptor.capture());

        // Verifies the case there isn't a fallback image is available.
        histogramWatcher =
                HistogramWatcher.newBuilder().expectBooleanRecord(histogramName, false).build();
        mFetchImageCallbackCaptor.getValue().onBitmap(null);
        verify(tile1, times(2)).setImageDrawable(any(Drawable.class));
        // Verifies that the tile isn't updated for salient image.
        verify(tile1).updateForSalientImage();
        histogramWatcher.assertExpected();

        // Verifies the case there is a fallback image available.
        histogramWatcher =
                HistogramWatcher.newBuilder().expectBooleanRecord(histogramName, false).build();
        mFetchImageCallbackCaptor.getValue().onBitmap(bitmap);
        verify(tile1, times(3)).setImageDrawable(any(Drawable.class));
        // Verifies that the tile isn't updated for salient image.
        verify(tile1).updateForSalientImage();
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRenderSingleLocalView() {
        initModuleView();
        TabResumptionModuleUtils.TAB_RESUMPTION_SHOW_DEFAULT_REASON.setForTesting(false);

        SuggestionEntry entry1 = SuggestionEntry.createFromLocalTab(mTab);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch favicon.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());

        // Capture call to fetch tab thumbnail.
        verify(mUrlImageProvider, atLeastOnce())
                .getTabThumbnail(
                        eq(TAB_ID), eq(mThumbnailSize), mThumbnailCallbackCaptor.capture());

        // Check tile texts.
        LocalTileView localTileView = (LocalTileView) mTileContainerView.getChildAt(0);
        // The default reason isn't shown.
        Assert.assertEquals(
                View.GONE, localTileView.findViewById(R.id.tab_show_reason).getVisibility());
        TextView titleView = localTileView.findViewById(R.id.tab_title_view);
        Assert.assertEquals(TAB_TITLE, titleView.getText());
        // Verifies that the maximum lines are the default 3 lines when the reason chip isn't shown.
        Assert.assertEquals(
                TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_DEFAULT, titleView.getMaxLines());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.one.com",
                ((TextView) localTileView.findViewById(R.id.tab_url_view)).getText());
        // Verifies that a placeholder icon drawable is set for the tab thumbnail.
        Assert.assertNotNull(
                ((TabThumbnailView) localTileView.findViewById(R.id.tab_thumbnail))
                        .getIconDrawableForTesting());

        // Provide test image, and check that it's shown as icon.
        Bitmap expectedBitmap = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(expectedBitmap);
        BitmapDrawable drawable =
                (BitmapDrawable)
                        ((ImageView) localTileView.findViewById(R.id.tab_favicon_view))
                                .getDrawable();
        Assert.assertNotNull(drawable);
        Assert.assertEquals(expectedBitmap, drawable.getBitmap());

        mThumbnailCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new BitmapDrawable(makeBitmap(64, 64)));
        // Verifies that the placeholder icon drawable is removed after setting a foreground bitmap.
        Assert.assertNull(
                ((TabThumbnailView) localTileView.findViewById(R.id.tab_thumbnail))
                        .getIconDrawableForTesting());

        // Simulate click on the local Tab.
        Assert.assertEquals(0, mClickCount);
        Assert.assertNull(mLastClickEntry);
        localTileView.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(TAB_ID, mLastClickEntry.getLocalTabId());
    }

    @Test
    @SmallTest
    public void testRenderSingleLocalViewWithDefaultReason() {
        initModuleView();
        TabResumptionModuleUtils.TAB_RESUMPTION_SHOW_DEFAULT_REASON.setForTesting(true);

        SuggestionEntry entry1 =
                new SuggestionEntry(
                        SuggestionEntryType.LOCAL_TAB,
                        /* sourceName= */ "",
                        mTab.getUrl(),
                        mTab.getTitle(),
                        makeTimestamp(24 - 3, 0, 0),
                        mTab.getId(),
                        /* appId= */ null,
                        /* reasonToShowTab= */ null,
                        /* needMatchLocalTab= */ false);
        mSuggestionBundle.entries.add(entry1);
        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch favicon.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());

        // Capture call to fetch tab thumbnail.
        verify(mUrlImageProvider, atLeastOnce())
                .getTabThumbnail(
                        eq(TAB_ID), eq(mThumbnailSize), mThumbnailCallbackCaptor.capture());

        LocalTileView localTileView = (LocalTileView) mTileContainerView.getChildAt(0);
        // The default reason is displayed.
        String expectedDefaultReason = "You visited 3 hr ago";
        TextView reasonView = localTileView.findViewById(R.id.tab_show_reason);
        Assert.assertEquals(View.VISIBLE, reasonView.getVisibility());
        Assert.assertEquals(expectedDefaultReason, reasonView.getText());

        // Verifies that the maximum lines are 2 lines instead of the default 3 lines when a reason
        // chip is shown.
        TextView titleView = localTileView.findViewById(R.id.tab_title_view);
        Assert.assertEquals(
                TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_WITH_REASON,
                titleView.getMaxLines());
        Assert.assertEquals(TAB_TITLE, titleView.getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.one.com",
                ((TextView) localTileView.findViewById(R.id.tab_url_view)).getText());
        // Verifies that a placeholder icon drawable is set for the tab thumbnail.
        Assert.assertNotNull(
                ((TabThumbnailView) localTileView.findViewById(R.id.tab_thumbnail))
                        .getIconDrawableForTesting());

        // Provide test image, and check that it's shown as icon.
        Bitmap expectedBitmap = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(expectedBitmap);
        BitmapDrawable drawable =
                (BitmapDrawable)
                        ((ImageView) localTileView.findViewById(R.id.tab_favicon_view))
                                .getDrawable();
        Assert.assertNotNull(drawable);
        Assert.assertEquals(expectedBitmap, drawable.getBitmap());

        mThumbnailCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new BitmapDrawable(makeBitmap(64, 64)));
        // Verifies that the placeholder icon drawable is removed after setting a foreground bitmap.
        Assert.assertNull(
                ((TabThumbnailView) localTileView.findViewById(R.id.tab_thumbnail))
                        .getIconDrawableForTesting());

        // Simulate click on the local Tab.
        Assert.assertEquals(0, mClickCount);
        Assert.assertNull(mLastClickEntry);
        localTileView.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(TAB_ID, mLastClickEntry.getLocalTabId());
    }

    @Test
    @SmallTest
    public void testRenderDouble() {
        initModuleView();

        SuggestionEntry entry1 =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "My Tablet",
                        /* url= */ JUnitTestGURLs.BLUE_3,
                        /* title= */ "Blue website with a very long title that might not fit",
                        /* timestamp= */ makeTimestamp(24 - 1, 60 - 16, 0));
        SuggestionEntry entry2 =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0));
        mSuggestionBundle.entries.add(entry1);
        mSuggestionBundle.entries.add(entry2);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(3, mTileContainerView.getChildCount()); // 2 tiles, 1 divider.

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(2, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(2, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(JUnitTestGURLs.BLUE_3, mFetchImagePageUrlCaptor.getAllValues().get(0));
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(1));

        // Check tiles texts, and presence of divider.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                "Blue website with a very long title that might not fit",
                ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "www.blue.com \u2022 My Tablet",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        View divider = (View) mTileContainerView.getChildAt(1);
        Assert.assertEquals(View.VISIBLE, divider.getVisibility());

        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        Assert.assertEquals(
                "Google Dog", ((TextView) tile2.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "www.google.com \u2022 Desktop",
                ((TextView) tile2.findViewById(R.id.tile_post_info_text)).getText());

        // Images are not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());
        Assert.assertNull(((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test images, and check that they're shown as icons.
        Bitmap bitmap1 = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        Bitmap bitmap2 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(1).onBitmap(bitmap2);
        BitmapDrawable drawable2 =
                (BitmapDrawable) ((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable2);
        Assert.assertEquals(bitmap2, drawable2.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertNull(mLastClickEntry);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.BLUE_3, mLastClickEntry.url);
    }

    @Test
    @SmallTest
    public void testRenderDoubleWithLocalTab() {
        initModuleView();

        SuggestionEntry entry1 = SuggestionEntry.createFromLocalTab(mTab);
        SuggestionEntry entry2 =
                SuggestionEntry.createFromForeignFields(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0));
        mSuggestionBundle.entries.add(entry1);
        mSuggestionBundle.entries.add(entry2);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(3, mTileContainerView.getChildCount()); // 2 tiles, 1 divider.

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(2, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(2, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(JUnitTestGURLs.URL_1, mFetchImagePageUrlCaptor.getAllValues().get(0));
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(1));

        // Check tiles texts, and presence of divider.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                TAB_TITLE, ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "www.one.com", ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        View divider = (View) mTileContainerView.getChildAt(1);
        Assert.assertEquals(View.VISIBLE, divider.getVisibility());

        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        Assert.assertEquals(
                "Google Dog", ((TextView) tile2.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "www.google.com \u2022 Desktop",
                ((TextView) tile2.findViewById(R.id.tile_post_info_text)).getText());

        // Images are not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());
        Assert.assertNull(((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test images, and check that they're shown as icons.
        Bitmap bitmap1 = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        Bitmap bitmap2 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(1).onBitmap(bitmap2);
        BitmapDrawable drawable2 =
                (BitmapDrawable) ((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable2);
        Assert.assertEquals(bitmap2, drawable2.getBitmap());

        Assert.assertEquals(0, mClickCount);
        Assert.assertNull(mLastClickEntry);

        // Simulate click on a local Tab.
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(TAB_ID, mLastClickEntry.getLocalTabId());

        // Simulate click on a remote Tab.
        tile2.performClick();
        Assert.assertEquals(2, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickEntry.url);
    }

    @Test
    @SmallTest
    public void testRenderSingleForHistoryData_Cct() throws Exception {
        initModuleView();

        final String appId = "com.google.android.youtube";
        final String appLabel = "YouTube";
        Drawable appIcon = new BitmapDrawable(mContext.getResources(), makeBitmap(32, 32));
        PackageManager packageManager = Mockito.mock(PackageManager.class);
        ApplicationInfo info = Mockito.mock(ApplicationInfo.class);
        when(packageManager.getApplicationInfo(eq(appId), anyInt())).thenReturn(info);
        when(packageManager.getApplicationIcon(any(ApplicationInfo.class))).thenReturn(appIcon);
        when(packageManager.getApplicationLabel(any(ApplicationInfo.class))).thenReturn(appLabel);
        mTileContainerView.setPackageManagerForTesting(packageManager);

        SuggestionEntry entry1 =
                new SuggestionEntry(
                        SuggestionEntryType.HISTORY,
                        "Device Source",
                        JUnitTestGURLs.GOOGLE_URL_DOG,
                        "Google Dog",
                        makeTimestamp(24 - 3, 0, 0),
                        Tab.INVALID_TAB_ID,
                        appId,
                        null,
                        /* needMatchLocalTab= */ false);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Chip view appears instead of the top title (From...).
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_pre_info_text).getVisibility());
        ChipView chipView = (ChipView) tile1.findViewById(R.id.tile_app_chip);
        var chipText =
                mContext.getResources().getString(R.string.history_app_attribution, appLabel);
        Assert.assertEquals("ChipView is not visible", View.VISIBLE, chipView.getVisibility());
        Assert.assertEquals(chipText, chipView.getPrimaryTextView().getText());
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.google.com \u2022 Device Source",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertNull(mLastClickEntry);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickEntry.url);
    }

    @Test
    @SmallTest
    public void testRenderSingleForHistoryData_BrApp() throws Exception {
        TabResumptionModuleUtils.TAB_RESUMPTION_SHOW_DEFAULT_REASON.setForTesting(false);
        initModuleView();

        SuggestionEntry entry1 =
                new SuggestionEntry(
                        SuggestionEntryType.HISTORY,
                        "Device Source",
                        JUnitTestGURLs.GOOGLE_URL_DOG,
                        "Google Dog",
                        makeTimestamp(24 - 3, 0, 0),
                        Tab.INVALID_TAB_ID,
                        null,
                        null,
                        /* needMatchLocalTab= */ false);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Neither pre_info/app chip is displayed.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_pre_info_text).getVisibility());
        // Verifies that the maximum lines are the default 3 lines for the display text.
        TextView displayTextView = tile1.findViewById(R.id.tile_display_text);
        Assert.assertEquals(
                TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_DEFAULT,
                displayTextView.getMaxLines());
        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_app_chip).getVisibility());
        Assert.assertEquals("Google Dog", displayTextView.getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.google.com \u2022 Device Source",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertNull(mLastClickEntry);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickEntry.url);
    }

    @Test
    @SmallTest
    public void testRenderSingleWithReasonToShowTab() throws Exception {
        initModuleView();

        SuggestionEntry entry1 =
                new SuggestionEntry(
                        SuggestionEntryType.HISTORY,
                        "Device Source",
                        JUnitTestGURLs.GOOGLE_URL_DOG,
                        "Google Dog",
                        makeTimestamp(24 - 3, 0, 0),
                        Tab.INVALID_TAB_ID,
                        null,
                        REASON_TO_SHOW_TAB,
                        /* needMatchLocalTab= */ false);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // The pre_info is displayed, while app chip isn't.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                View.VISIBLE, tile1.findViewById(R.id.tile_pre_info_text).getVisibility());
        Assert.assertEquals(
                REASON_TO_SHOW_TAB,
                ((TextView) tile1.findViewById(R.id.tile_pre_info_text)).getText());

        // Verifies that the maximum lines are 2 lines instead of the default 3 lines when a reason
        // chip is shown.
        TextView displayTextView = tile1.findViewById(R.id.tile_display_text);
        Assert.assertEquals(
                TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_WITH_REASON,
                displayTextView.getMaxLines());

        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_app_chip).getVisibility());
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.google.com \u2022 Device Source",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());
    }

    @Test
    @SmallTest
    public void testRenderSingleWithDefaultReason() throws Exception {
        initModuleView();
        TabResumptionModuleUtils.TAB_RESUMPTION_SHOW_DEFAULT_REASON.setForTesting(true);

        SuggestionEntry entry1 =
                new SuggestionEntry(
                        SuggestionEntryType.HISTORY,
                        "Device Source",
                        JUnitTestGURLs.GOOGLE_URL_DOG,
                        "Google Dog",
                        makeTimestamp(24 - 3, 0, 0),
                        Tab.INVALID_TAB_ID,
                        null,
                        null,
                        /* needMatchLocalTab= */ false);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // The pre_info is displayed, while app chip isn't.
        String expectedDefaultReason = "You visited 3 hr ago";
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                View.VISIBLE, tile1.findViewById(R.id.tile_pre_info_text).getVisibility());
        Assert.assertEquals(
                expectedDefaultReason,
                ((TextView) tile1.findViewById(R.id.tile_pre_info_text)).getText());

        // Verifies that the maximum lines are 2 lines instead of the default 3 lines when a reason
        // chip is shown.
        TextView displayTextView = tile1.findViewById(R.id.tile_display_text);
        Assert.assertEquals(
                TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_WITH_REASON,
                displayTextView.getMaxLines());

        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_app_chip).getVisibility());
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.google.com \u2022 Device Source",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());
    }

    @Test
    @SmallTest
    public void testHistoryDataMatchesTrackingTab() throws Exception {
        TabResumptionModuleUtils.TAB_RESUMPTION_FETCH_HISTORY_BACKEND.setForTesting(true);
        initModuleView();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        SuggestionEntry entry1 =
                new SuggestionEntry(
                        SuggestionEntryType.HISTORY,
                        "Device Source",
                        JUnitTestGURLs.BLUE_1,
                        "Google Dog",
                        makeTimestamp(24 - 3, 0, 0),
                        Tab.INVALID_TAB_ID,
                        null,
                        null,
                        /* needMatchLocalTab= */ true);

        assertTrue(entry1.url.equals(mTrackingTab.getUrl()));
        assertTrue(entry1.getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Verifies that a LocalTileView is created for the history suggestion and is updated to
        // match the trackingTab.
        LocalTileView localTileView = (LocalTileView) mTileContainerView.getChildAt(0);
        verifyTileMatchesALocalTab(
                entry1,
                mTrackingTab,
                localTileView,
                /* shouldUpdateModuleShowConfig= */ true,
                /* expectedModuleShowConfig= */ ModuleShowConfig.SINGLE_TILE_LOCAL,
                ClickInfo.LOCAL_SINGLE_FIRST,
                /* initialClickCount= */ 0,
                "www.blue.com");

        // Capture call to fetch favicon.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());

        // Capture call to fetch tab thumbnail.
        verify(mUrlImageProvider, atLeastOnce())
                .getTabThumbnail(
                        eq(TRACKING_TAB_ID),
                        eq(mThumbnailSize),
                        mThumbnailCallbackCaptor.capture());

        // Check tile texts. The default reason isn't shown.
        Assert.assertEquals(
                View.GONE, localTileView.findViewById(R.id.tab_show_reason).getVisibility());
        TextView titleView = localTileView.findViewById(R.id.tab_title_view);
        Assert.assertEquals("Google Dog", titleView.getText());
        // Verifies that the maximum lines are the default 3 lines when the reason chip isn't shown.
        Assert.assertEquals(
                TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_DEFAULT, titleView.getMaxLines());
        // Verifies that a placeholder icon drawable is set for the tab thumbnail.
        Assert.assertNotNull(
                ((TabThumbnailView) localTileView.findViewById(R.id.tab_thumbnail))
                        .getIconDrawableForTesting());

        // Provide test image, and check that it's shown as icon.
        Bitmap expectedBitmap = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(expectedBitmap);
        BitmapDrawable drawable =
                (BitmapDrawable)
                        ((ImageView) localTileView.findViewById(R.id.tab_favicon_view))
                                .getDrawable();
        Assert.assertNotNull(drawable);
        Assert.assertEquals(expectedBitmap, drawable.getBitmap());

        mThumbnailCallbackCaptor
                .getAllValues()
                .get(0)
                .onResult(new BitmapDrawable(makeBitmap(64, 64)));
        // Verifies that the placeholder icon drawable is removed after setting a foreground bitmap.
        Assert.assertNull(
                ((TabThumbnailView) localTileView.findViewById(R.id.tab_thumbnail))
                        .getIconDrawableForTesting());
    }

    @Test
    @SmallTest
    public void testHistoryDataMatchesNoneTrackingTab() throws Exception {
        TabResumptionModuleUtils.TAB_RESUMPTION_FETCH_HISTORY_BACKEND.setForTesting(true);
        initModuleView();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        SuggestionEntry entry1 =
                new SuggestionEntry(
                        SuggestionEntryType.HISTORY,
                        "Device Source",
                        JUnitTestGURLs.URL_1,
                        "Google Dog",
                        makeTimestamp(24 - 3, 0, 0),
                        Tab.INVALID_TAB_ID,
                        null,
                        null,
                        /* needMatchLocalTab= */ true);
        // The entry1 doesn't match the tracking Tab.
        assertFalse(entry1.url.equals(mTrackingTab.getUrl().getSpec()));
        assertTrue(entry1.getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        assertEquals(Tab.INVALID_TAB_ID, entry1.getLocalTabId());
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Verifies that a TabResumptionTileView is created for the history suggestion.
        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(JUnitTestGURLs.URL_1, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Neither pre_info/app chip is displayed.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_pre_info_text).getVisibility());
        // Verifies that the maximum lines are the default 3 lines for the display text.
        TextView displayTextView = tile1.findViewById(R.id.tile_display_text);
        Assert.assertEquals(
                TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_DEFAULT,
                displayTextView.getMaxLines());
        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_app_chip).getVisibility());
        Assert.assertEquals("Google Dog", displayTextView.getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.one.com \u2022 Device Source",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        verifyClickingNotMatchedTile(
                entry1,
                tile1,
                ClickInfo.HISTORY_SINGLE_FIRST,
                /* initialClickCount= */ 0,
                /* isTileUpdated= */ false,
                "www.one.com \u2022 Device Source");

        // Sets the TabModel to make entry1 matches the other Tab in the model.
        mTabsInTabModel.add(mTrackingTab);
        mTabsInTabModel.add(mTab);
        initializeTabState(mTabsInTabModel);

        // Verifies that the entry1 is updated to match mTab after the tab state initialization is
        // completed.
        verifyTileMatchesALocalTab(
                entry1,
                mTab,
                tile1,
                /* shouldUpdateModuleShowConfig= */ true,
                ModuleShowConfig.SINGLE_TILE_LOCAL,
                ClickInfo.LOCAL_SINGLE_FIRST,
                /* initialClickCount= */ 1,
                "www.one.com");
    }

    @Test
    @SmallTest
    public void testHistoryDataDoesNotMatchesAnyLocalTab() throws Exception {
        TabResumptionModuleUtils.TAB_RESUMPTION_FETCH_HISTORY_BACKEND.setForTesting(true);
        initModuleView();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        SuggestionEntry entry1 =
                new SuggestionEntry(
                        SuggestionEntryType.HISTORY,
                        "Device Source",
                        JUnitTestGURLs.URL_2,
                        "Google Dog",
                        makeTimestamp(24 - 3, 0, 0),
                        Tab.INVALID_TAB_ID,
                        null,
                        null,
                        /* needMatchLocalTab= */ true);
        // The entry1 doesn't match the tracking Tab.
        assertFalse(entry1.url.equals(mTrackingTab.getUrl().getSpec()));
        assertTrue(entry1.getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        assertEquals(entry1.getLocalTabId(), Tab.INVALID_TAB_ID);
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Verifies that a TabResumptionTileView is created for the history suggestion.
        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(JUnitTestGURLs.URL_2, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Neither pre_info/app chip is displayed.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_pre_info_text).getVisibility());
        // Verifies that the maximum lines are the default 3 lines for the display text.
        TextView displayTextView = tile1.findViewById(R.id.tile_display_text);
        Assert.assertEquals(
                TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_DEFAULT,
                displayTextView.getMaxLines());
        Assert.assertEquals(View.GONE, tile1.findViewById(R.id.tile_app_chip).getVisibility());
        Assert.assertEquals("Google Dog", displayTextView.getText());
        // Actual code would remove "www." prefix, but the test's JNI mock doesn't do so.
        Assert.assertEquals(
                "www.two.com \u2022 Device Source",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        verifyClickingNotMatchedTile(
                entry1,
                tile1,
                ClickInfo.HISTORY_SINGLE_FIRST,
                /* initialClickCount= */ 0,
                /* isTileUpdated= */ false,
                "www.two.com \u2022 Device Source");

        mTabsInTabModel.add(mTrackingTab);
        mTabsInTabModel.add(mTab);
        initializeTabState(mTabsInTabModel);

        // Verifies that the entry1 and its tile aren't updated.
        verifyClickingNotMatchedTile(
                entry1,
                tile1,
                ClickInfo.HISTORY_SINGLE_FIRST,
                /* initialClickCount= */ 1,
                /* isTileUpdated= */ true,
                "www.two.com \u2022 Device Source");
    }

    @Test
    @SmallTest
    public void testHistoryDataTwoTiles_FirstMatchesTrackingTab() {
        initModuleView();
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);

        SuggestionEntry[] entries =
                createTwoHistoryTiles(
                        mTrackingTab.getUrl(),
                        mTab.getUrl(),
                        /* needMatchLocalTab1= */ true,
                        /* needMatchLocalTab2= */ true);

        assertTrue(entries[0].url.equals(mTrackingTab.getUrl()));
        assertTrue(entries[0].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[0]);
        assertFalse(entries[1].url.equals(mTrackingTab.getUrl()));
        assertTrue(entries[1].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[1]);
        Assert.assertEquals(0, mTileContainerView.getChildCount());
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Renders tiles:
        mModuleView.setSuggestionBundle(mSuggestionBundle);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        // There is an extra view between the two tile views.
        Assert.assertEquals(3, mTileContainerView.getChildCount());

        // Verifies that entries[0] has been updated to match the tracking Tab.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        verifyTileMatchesALocalTab(
                entries[0],
                mTrackingTab,
                tile1,
                /* shouldUpdateModuleShowConfig= */ false,
                /* expectedModuleShowConfig= */ null,
                ClickInfo.LOCAL_DOUBLE_ANY,
                /* initialClickCount= */ 0,
                "www.blue.com");

        // Sets the TabModel to make entries[1] doesn't match any Tab in the model.
        mTabsInTabModel.add(mTrackingTab);
        initializeTabState(mTabsInTabModel);

        // Verifies that the entries[1] and its tile are updated  after the tab state initialization
        // is completed.
        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        verifyNotMatchedTileIsUpdated(
                entries[1],
                tile2,
                /* shouldUpdateModuleShowConfig= */ true,
                ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY,
                ClickInfo.HISTORY_DOUBLE_ANY,
                /* initialClickCount= */ 1);
    }

    @Test
    @SmallTest
    public void testHistoryDataTwoTiles_SecondMatchesTrackingTab() throws Exception {
        initModuleView();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        SuggestionEntry[] entries =
                createTwoHistoryTiles(
                        JUnitTestGURLs.URL_2,
                        mTrackingTab.getUrl(),
                        /* needMatchLocalTab1= */ true,
                        /* needMatchLocalTab2= */ true);

        assertFalse(entries[0].url.equals(mTrackingTab.getUrl()));
        assertTrue(entries[0].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[0]);
        assertTrue(entries[1].url.equals(mTrackingTab.getUrl()));
        assertTrue(entries[1].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[1]);
        assertEquals(2, mSuggestionBundle.entries.size());
        Assert.assertEquals(0, mTileContainerView.getChildCount());
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Renders tiles:
        mModuleView.setSuggestionBundle(mSuggestionBundle);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        // There is an extra view between the two tile views.
        Assert.assertEquals(3, mTileContainerView.getChildCount());

        // Verifies that entries[1] has been updated to match the tracking Tab.
        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        verifyTileMatchesALocalTab(
                entries[1],
                mTrackingTab,
                tile2,
                /* shouldUpdateModuleShowConfig= */ false,
                /* expectedModuleShowConfig= */ null,
                ClickInfo.LOCAL_DOUBLE_ANY,
                /* initialClickCount= */ 0,
                "www.blue.com");

        // Sets the TabModel to make entries[0] doesn't match any Tab in the model.
        mTabsInTabModel.add(mTrackingTab);
        initializeTabState(mTabsInTabModel);

        // Verifies that the entries[0] and its tile are updated after the tab state initialization
        // is completed.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        verifyNotMatchedTileIsUpdated(
                entries[0],
                tile1,
                /* shouldUpdateModuleShowConfig= */ true,
                ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY,
                ClickInfo.HISTORY_DOUBLE_ANY,
                /* initialClickCount= */ 1);
    }

    @Test
    @SmallTest
    public void testHistoryDataTwoTiles_BothMatchLocalTabs() {
        initModuleView();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        SuggestionEntry[] entries =
                createTwoHistoryTiles(
                        mTrackingTab.getUrl(),
                        mTab.getUrl(),
                        /* needMatchLocalTab1= */ true,
                        /* needMatchLocalTab2= */ true);

        assertTrue(entries[0].url.equals(mTrackingTab.getUrl()));
        assertTrue(entries[0].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[0]);
        assertTrue(entries[1].url.equals(mTab.getUrl()));
        assertTrue(entries[1].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[1]);
        Assert.assertEquals(0, mTileContainerView.getChildCount());
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Renders tiles:
        mModuleView.setSuggestionBundle(mSuggestionBundle);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        // There is an extra view between the two tile views.
        Assert.assertEquals(3, mTileContainerView.getChildCount());

        // Verifies that entries[0] has been updated to match the tracking Tab.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        verifyTileMatchesALocalTab(
                entries[0],
                mTrackingTab,
                tile1,
                /* shouldUpdateModuleShowConfig= */ false,
                /* expectedModuleShowConfig= */ null,
                ClickInfo.LOCAL_DOUBLE_ANY,
                /* initialClickCount= */ 0,
                "www.blue.com");

        // Verifies that tiles2 still needs to be matched and isn't updated.
        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        verifyClickingNotMatchedTile(
                entries[1],
                tile2,
                ClickInfo.HISTORY_DOUBLE_ANY,
                /* initialClickCount= */ 1,
                /* isTileUpdated= */ false,
                "www.one.com \u2022 Device Source");

        // Sets the TabModel to make entries[1] match the other Tab in the model.
        mTabsInTabModel.add(mTrackingTab);
        mTabsInTabModel.add(mTab);
        initializeTabState(mTabsInTabModel);

        // Verifies that the entries[1] matches mTab after the tab state initialization is
        // completed.
        verifyTileMatchesALocalTab(
                entries[1],
                mTab,
                tile2,
                /* shouldUpdateModuleShowConfig= */ true,
                ModuleShowConfig.DOUBLE_TILE_LOCAL_LOCAL,
                ClickInfo.LOCAL_DOUBLE_ANY,
                /* initialClickCount= */ 2,
                "www.one.com");
    }

    @Test
    @SmallTest
    public void testHistoryDataTwoTiles_NoneMatchLocalTabs() {
        initModuleView();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        SuggestionEntry[] entries =
                createTwoHistoryTiles(
                        JUnitTestGURLs.URL_2,
                        JUnitTestGURLs.URL_3,
                        /* needMatchLocalTab1= */ true,
                        /* needMatchLocalTab2= */ true);

        assertFalse(entries[0].url.equals(mTrackingTab.getUrl()));
        assertTrue(entries[0].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[0]);
        assertFalse(entries[1].url.equals(mTrackingTab.getUrl()));
        assertTrue(entries[1].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[1]);
        Assert.assertEquals(0, mTileContainerView.getChildCount());
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Renders tiles:
        mModuleView.setSuggestionBundle(mSuggestionBundle);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());

        // There is an extra view between the two tile views.
        Assert.assertEquals(3, mTileContainerView.getChildCount());

        // Verifies that no entries are updated.
        assertEquals(Tab.INVALID_TAB_ID, entries[1].getLocalTabId());
        assertTrue(entries[1].getNeedMatchLocalTab());
        assertEquals(Tab.INVALID_TAB_ID, entries[0].getLocalTabId());
        assertTrue(entries[0].getNeedMatchLocalTab());
        verify(mTabObserverCallback, never()).onResult(eq(mTrackingTab));
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Sets the TabModel to make both entries don't match any Tab in the model.
        mTabsInTabModel.add(mTrackingTab);
        initializeTabState(mTabsInTabModel);

        // Verifies that both entries are updated after the tab state initialization is completed.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        verifyNotMatchedTileIsUpdated(
                entries[0],
                tile1,
                /* shouldUpdateModuleShowConfig= */ true,
                ModuleShowConfig.DOUBLE_TILE_HISTORY_HISTORY,
                ClickInfo.HISTORY_DOUBLE_ANY,
                /* initialClickCount= */ 0);

        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        verifyNotMatchedTileIsUpdated(
                entries[1],
                tile2,
                /* shouldUpdateModuleShowConfig= */ true,
                ModuleShowConfig.DOUBLE_TILE_HISTORY_HISTORY,
                ClickInfo.HISTORY_DOUBLE_ANY,
                /* initialClickCount= */ 1);
    }

    @Test
    @SmallTest
    public void testHistoryDataTwoTiles_FirstMatchedTrackingTab_SecondDoesNotNeedToMatch() {
        initModuleView();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        SuggestionEntry[] entries =
                createTwoHistoryTiles(
                        JUnitTestGURLs.BLUE_1,
                        JUnitTestGURLs.URL_2,
                        /* needMatchLocalTab1= */ true,
                        /* needMatchLocalTab2= */ false);

        assertTrue(entries[0].url.equals(mTrackingTab.getUrl()));
        assertTrue(entries[0].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[0]);
        assertFalse(entries[1].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[1]);
        Assert.assertEquals(0, mTileContainerView.getChildCount());
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Renders tiles:
        mModuleView.setSuggestionBundle(mSuggestionBundle);
        verify(mTabModelSelector, never()).addObserver(mTabModelSelectorObserverCaptor.capture());
        // There is an extra view between the two tile views.
        Assert.assertEquals(3, mTileContainerView.getChildCount());

        // Verifies that entries[0] has been updated to match the tracking Tab.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        verifyTileMatchesALocalTab(
                entries[0],
                mTrackingTab,
                tile1,
                /* shouldUpdateModuleShowConfig= */ true,
                ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY,
                ClickInfo.LOCAL_DOUBLE_ANY,
                /* initialClickCount= */ 0,
                "www.blue.com");

        // Simulate click on the second tile view which doesn't need to match a local Tab.
        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        verifyClickingNotMatchedTile(
                entries[1],
                tile2,
                ClickInfo.HISTORY_DOUBLE_ANY,
                /* initialClickCount= */ 1,
                /* isTileUpdated= */ true,
                "www.two.com \u2022 Device Source");
    }

    @Test
    @SmallTest
    public void testHistoryDataTwoTiles_FirstDoesNotNeedToMatch_SecondMatchesLocalTab() {
        initModuleView();

        when(mTabModelSelector.isTabStateInitialized()).thenReturn(false);
        SuggestionEntry[] entries =
                createTwoHistoryTiles(
                        JUnitTestGURLs.URL_2,
                        mTab.getUrl(),
                        /* needMatchLocalTab1= */ false,
                        /* needMatchLocalTab2= */ true);

        assertFalse(entries[0].url.equals(mTrackingTab.getUrl()));
        assertFalse(entries[0].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[0]);
        assertTrue(entries[1].url.equals(mTab.getUrl()));
        assertTrue(entries[1].getNeedMatchLocalTab());
        mSuggestionBundle.entries.add(entries[1]);
        Assert.assertEquals(0, mTileContainerView.getChildCount());

        // Renders tiles:
        mModuleView.setSuggestionBundle(mSuggestionBundle);
        verify(mTabModelSelector).addObserver(mTabModelSelectorObserverCaptor.capture());
        // There is an extra view between the two tile views.
        Assert.assertEquals(3, mTileContainerView.getChildCount());
        verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());

        // Sets the TabModel to make entries[1] match mTab.
        mTabsInTabModel.add(mTrackingTab);
        mTabsInTabModel.add(mTab);
        initializeTabState(mTabsInTabModel);

        // Verifies that entries[1] has been updated to match the mTab.
        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        verifyTileMatchesALocalTab(
                entries[1],
                mTab,
                tile2,
                /* shouldUpdateModuleShowConfig= */ true,
                ModuleShowConfig.DOUBLE_TILE_LOCAL_HISTORY,
                ClickInfo.LOCAL_DOUBLE_ANY,
                /* initialClickCount= */ 0,
                "www.one.com");

        // Simulate click on the entries[0] which doesn't match a local Tab.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        verifyClickingNotMatchedTile(
                entries[0],
                tile1,
                ClickInfo.HISTORY_DOUBLE_ANY,
                /* initialClickCount= */ 1,
                /* isTileUpdated= */ true,
                "www.two.com \u2022 Device Source");
    }

    private void initModuleView() {
        mModuleView =
                (TabResumptionModuleView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.tab_resumption_module_layout, null);

        mModuleView.setUrlImageProvider(mUrlImageProvider);
        mModuleView.setClickCallback(mClickCallback);
        mModuleView.setTabModelSelectorSupplier(mTabModelSelectorSupplier);
        mModuleView.setTabObserverCallback(mTabObserverCallback);
        mModuleView.setTrackingTab(mTrackingTab);
        mModuleView.setOnModuleShowConfigFinalizedCallback(mOnModuleShowConfigFinalizedCallback);
        mTileContainerView = mModuleView.getTileContainerViewForTesting();
    }

    /**
     * Verifies the suggestionEntry and click listener have been updated for a tile which match a
     * local Tab.
     *
     * @param entry The {@link SuggestionEntry} instance.
     * @param matchedTab The matched Tab found.
     * @param tile The {@link TabResumptionTileView} instance.
     * @param shouldUpdateModuleShowConfig Whether the callback to update ModuleShowConfig should be
     *     called.
     * @param expectedModuleShowConfig The expected type of ModuleShowConfig.
     * @param expectedClickInfo The expected clickInfo to be recorded when clicking the tile.
     * @param initialClickCount The initial count of a click.
     * @param expectedPostInfoText The expected post info text to display.
     */
    private void verifyTileMatchesALocalTab(
            SuggestionEntry entry,
            Tab matchedTab,
            View tile,
            boolean shouldUpdateModuleShowConfig,
            @Nullable @ModuleShowConfig Integer expectedModuleShowConfig,
            @ClickInfo int expectedClickInfo,
            int initialClickCount,
            String expectedPostInfoText) {
        // Verifies that entry has been updated to match the matchedTab.
        assertEquals(matchedTab.getId(), entry.getLocalTabId());
        assertFalse(entry.getNeedMatchLocalTab());
        verify(mTabObserverCallback).onResult(eq(matchedTab));
        if (shouldUpdateModuleShowConfig) {
            // Verifies that the callback to update the ModuleShowConfig is called.
            verify(mOnModuleShowConfigFinalizedCallback).onResult(expectedModuleShowConfig);
        } else {
            verify(mOnModuleShowConfigFinalizedCallback, never()).onResult(anyInt());
        }

        // Verifies that the device info is removed for the tile.
        if (tile instanceof LocalTileView) {
            Assert.assertEquals(
                    expectedPostInfoText,
                    ((TextView) tile.findViewById(R.id.tab_url_view)).getText());
        } else {
            Assert.assertEquals(
                    expectedPostInfoText,
                    ((TextView) tile.findViewById(R.id.tile_post_info_text)).getText());
        }

        // Simulate click on the tile view which matches the matchedTab.
        String histogramName = "MagicStack.Clank.TabResumption.ClickInfo";
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, expectedClickInfo)
                        .build();
        Assert.assertEquals(initialClickCount, mClickCount);
        tile.performClick();
        Assert.assertEquals(initialClickCount + 1, mClickCount);
        Assert.assertEquals(matchedTab.getId(), mLastClickEntry.getLocalTabId());
        histogramWatcher.assertExpected();
    }

    /**
     * Verifies that the suggestionEntry has been updated for a tile which doesn't match any local
     * Tab, and verifies its clicking behaviour.
     *
     * @param entry The {@link SuggestionEntry} instance.
     * @param tile The {@link TabResumptionTileView} instance.
     * @param shouldUpdateModuleShowConfig Whether the callback to update ModuleShowConfig should be
     *     called.
     * @param expectedModuleShowConfig The expected type of ModuleShowConfig.
     * @param expectedClickInfo The expected clickInfo to be recorded when clicking the tile.
     * @param initialClickCount The initial count of a click.
     */
    private void verifyNotMatchedTileIsUpdated(
            SuggestionEntry entry,
            TabResumptionTileView tile,
            boolean shouldUpdateModuleShowConfig,
            @Nullable @ModuleShowConfig Integer expectedModuleShowConfig,
            @ClickInfo int expectedClickInfo,
            int initialClickCount) {
        // Verifies that the entry and its tile are updated.
        assertEquals(Tab.INVALID_TAB_ID, entry.getLocalTabId());
        assertFalse(entry.getNeedMatchLocalTab());
        if (shouldUpdateModuleShowConfig) {
            // Verifies that the callback to update the ModuleShowConfig is called.
            verify(mOnModuleShowConfigFinalizedCallback).onResult(expectedModuleShowConfig);
        } else {
            verify(mOnModuleShowConfigFinalizedCallback).onResult(anyInt());
        }

        // Simulate click on the tile view which doesn't match any local Tab.
        String histogramName = "MagicStack.Clank.TabResumption.ClickInfo";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, expectedClickInfo)
                        .build();
        Assert.assertEquals(initialClickCount, mClickCount);
        tile.performClick();
        Assert.assertEquals(initialClickCount + 1, mClickCount);
        Assert.assertEquals(Tab.INVALID_TAB_ID, mLastClickEntry.getLocalTabId());
        histogramWatcher.assertExpected();
    }

    /**
     * Verifies the clicking behaviour on a tile which doesn't need to match any local tab.
     *
     * @param entry The {@link SuggestionEntry} instance.
     * @param tile The {@link TabResumptionTileView} instance.
     * @param expectedClickInfo The expected clickInfo to be recorded when clicking the tile.
     * @param initialClickCount The initial count of a click.
     * @param isTileUpdated Whether the SuggestionEntry of the tile is expected to be updated.
     * @param expectedPostInfoText The expected post info text to display.
     */
    private void verifyClickingNotMatchedTile(
            SuggestionEntry entry,
            TabResumptionTileView tile,
            @ClickInfo int expectedClickInfo,
            int initialClickCount,
            boolean isTileUpdated,
            String expectedPostInfoText) {
        // Verifies that the entry and its tile are updated.
        assertEquals(Tab.INVALID_TAB_ID, entry.getLocalTabId());
        if (isTileUpdated) {
            assertFalse(entry.getNeedMatchLocalTab());
        }
        // Verifies that the device info was added for the tile.
        Assert.assertEquals(
                expectedPostInfoText,
                ((TextView) tile.findViewById(R.id.tile_post_info_text)).getText());

        // Simulate click on the tile view which doesn't match any local Tab.
        String histogramName = "MagicStack.Clank.TabResumption.ClickInfo";
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(histogramName, expectedClickInfo)
                        .build();
        Assert.assertEquals(initialClickCount, mClickCount);
        tile.performClick();
        Assert.assertEquals(initialClickCount + 1, mClickCount);
        Assert.assertEquals(Tab.INVALID_TAB_ID, mLastClickEntry.getLocalTabId());
        histogramWatcher.assertExpected();
    }

    /**
     * Adds the provided tabs to the TabModel, and notifies the onTabStateInitialized() event.
     *
     * @param tabs The list of Tabs to add to the TabModel.
     */
    private void initializeTabState(List<Tab> tabs) {
        when(mTabModel.getCount()).thenReturn(tabs.size());
        for (int i = 0; i < tabs.size(); i++) {
            when(mTabModel.getTabAt(i)).thenReturn(tabs.get(i));
        }
        mTabModelSelectorObserverCaptor.getValue().onTabStateInitialized();
    }
}
