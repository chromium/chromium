// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.refEq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Robolectric.buildActivity;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView.ScaleType;
import android.widget.TextView;

import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;
import androidx.core.content.ContextCompat;

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
import org.chromium.base.SysUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.tabs.TabAlert;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabHoverCardView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class TabHoverCardViewUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor private ArgumentCaptor<Callback<Bitmap>> mGetThumbnailCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Long>> mMemoryUsageCallbackCaptor;
    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    @Mock private Tab mHoveredTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabContentManager mTabContentManager;

    private final SettableMonotonicObservableSupplier<TabContentManager>
            mTabContentManagerSupplier = ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<TabModel> mTabModelSupplier =
            ObservableSuppliers.createMonotonic();

    // Used as a @Spy.
    private TabHoverCardView mTabHoverCardView;
    private ViewGroup mContentView;
    private TabThumbnailView mThumbnailView;
    private TextView mTitleView;
    private TextView mUrlView;
    private TextView mAlertStatusView;
    private TextView mMemoryUsageView;
    private Context mContext;
    private Bitmap mBitmap;
    private int mHoverCardWidth;

    @Before
    public void setUp() {
        mTabContentManagerSupplier.set(mTabContentManager);

        Activity activity = buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        var tabHoverCardView =
                (TabHoverCardView)
                        activity.getLayoutInflater().inflate(R.layout.tab_hover_card_holder, null);
        mTabHoverCardView = spy(tabHoverCardView);
        mContentView = mTabHoverCardView.findViewById(R.id.content_view);
        mThumbnailView = mTabHoverCardView.findViewById(R.id.thumbnail);
        mTitleView = mTabHoverCardView.findViewById(R.id.title);
        mUrlView = mTabHoverCardView.findViewById(R.id.url);
        mAlertStatusView = mTabHoverCardView.findViewById(R.id.alert_status);
        mMemoryUsageView = mTabHoverCardView.findViewById(R.id.memory_usage);

        mContext = mTabHoverCardView.getContext();
        mContext.getResources().getDisplayMetrics().density = 1f;

        when(mTabModelSelector.getCurrentTabModelSupplier()).thenReturn(mTabModelSupplier);
        mTabHoverCardView.initialize(mTabModelSelector, mTabContentManagerSupplier);
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);

        mHoverCardWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.tab_hover_card_width);
        int thumbnailHeight =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.tab_hover_card_thumbnail_height);
        mThumbnailView.measure(mHoverCardWidth, thumbnailHeight);
        mThumbnailView.layout(0, 0, mHoverCardWidth, thumbnailHeight);

        var originalLayoutParams = new LayoutParams((int) mHoverCardWidth, 200);
        when(mTabHoverCardView.getLayoutParams()).thenReturn(originalLayoutParams);

        SysUtils.setIsLowEndDeviceForTesting(false);
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void show() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);

        assertEquals("Card title text is incorrect.", mHoveredTab.getTitle(), mTitleView.getText());
        assertEquals(
                "Card URL text is incorrect.", mHoveredTab.getUrl().getHost(), mUrlView.getText());
        assertEquals(
                "|mLastHoveredTabId| is incorrect.",
                1,
                mTabHoverCardView.getLastHoveredTabIdForTesting());
        assertTrue("|mIsShowing| should be true.", mTabHoverCardView.isShowingForTesting());
        verify(mTabHoverCardView).setX(10f);
        verify(mTabHoverCardView).setY(20f);
        verify(mTabHoverCardView).setVisibility(eq(View.VISIBLE));

        verify(mTabContentManager)
                .getTabThumbnailWithCallback(
                        anyInt(),
                        refEq(new Size(mThumbnailView.getWidth(), mThumbnailView.getHeight())),
                        mGetThumbnailCallbackCaptor.capture());
        mGetThumbnailCallbackCaptor.getValue().onResult(mBitmap);

        assertEquals(
                "Thumbnail scale type is incorrect.",
                ScaleType.MATRIX,
                mThumbnailView.getScaleType());
        assertNotNull("Thumbnail image matrix should be set.", mThumbnailView.getImageMatrix());
        assertEquals(
                "Thumbnail image bitmap is incorrect.",
                mBitmap,
                ((BitmapDrawable) mThumbnailView.getDrawable()).getBitmap());
    }

    @Test
    public void show_AlertStatus() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);
        when(mHoveredTab.getAlertState()).thenReturn(TabAlert.GLIC_SHARING);

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);

        assertEquals(
                "Alert status view should be visible.",
                View.VISIBLE,
                mAlertStatusView.getVisibility());
        assertEquals(
                "Alert status text is incorrect.",
                mContext.getString(R.string.tooltip_tab_alert_state_glic_sharing),
                mAlertStatusView.getText().toString());
        assertNotNull(
                "Alert status icon should be present.",
                mAlertStatusView.getCompoundDrawablesRelative()[0]);

        // Verify alert status is gone when tab has no alert.
        when(mHoveredTab.getAlertState()).thenReturn(null);
        mTabHoverCardView.show(mHoveredTab, 10f, 20f);
        assertEquals(
                "Alert status view should be hidden when alert state is null.",
                View.GONE,
                mAlertStatusView.getVisibility());
    }

    @Test
    public void show_MemoryUsage() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);

        verify(mHoveredTab).getMemoryUsageBytes(mMemoryUsageCallbackCaptor.capture());

        long bytes = 100_000_000; // 100 MB in decimal (base 1000) as used by Android's Formatter.
        mMemoryUsageCallbackCaptor.getValue().onResult(bytes);

        String expectedMemoryText =
                mContext.getString(
                        R.string.tab_hover_card_memory_usage,
                        android.text.format.Formatter.formatShortFileSize(mContext, bytes));
        assertEquals(
                "Memory usage text is incorrect.",
                expectedMemoryText,
                mMemoryUsageView.getText().toString());
        assertEquals(
                "Memory usage view should be visible.",
                View.VISIBLE,
                mMemoryUsageView.getVisibility());
    }

    @Test
    public void show_ThumbnailScalesWithCardWidth() {
        // Set window width to be slightly smaller than the default card width.
        int windowWidth = (int) (mHoverCardWidth - 1);
        mContext.getResources().getDisplayMetrics().widthPixels = windowWidth;
        int expectedCardWidth = Math.round(0.9f * windowWidth);

        LayoutParams layoutParams = new LayoutParams(expectedCardWidth, 200);
        when(mTabHoverCardView.getLayoutParams()).thenReturn(layoutParams);

        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        // Ensure mThumbnailView has layout params to avoid crash.
        if (mThumbnailView.getLayoutParams() == null) {
            mThumbnailView.setLayoutParams(new ViewGroup.LayoutParams(0, 0));
        }

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);

        float hoverCardDefaultWidthPx =
                mContext.getResources().getDimension(R.dimen.tab_hover_card_width);
        float hoverCardThumbnailDefaultHeightPx =
                mContext.getResources().getDimension(R.dimen.tab_hover_card_thumbnail_height);
        int expectedThumbnailHeight =
                Math.round(
                        (float) expectedCardWidth
                                / hoverCardDefaultWidthPx
                                * hoverCardThumbnailDefaultHeightPx);

        // Verify thumbnail layout params height is scaled.
        assertEquals(
                "Thumbnail height is incorrect.",
                expectedThumbnailHeight,
                mThumbnailView.getLayoutParams().height);

        // Verify TabContentManager is called with the scaled size.
        verify(mTabContentManager)
                .getTabThumbnailWithCallback(
                        eq(1), refEq(new Size(expectedCardWidth, expectedThumbnailHeight)), any());
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void hoveredTabUsesChromeScheme() {
        var url = JUnitTestGURLs.NTP_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);

        assertEquals("Card title text is incorrect.", mHoveredTab.getTitle(), mTitleView.getText());
        // Verify chrome:// tab hover card display text.
        assertEquals(
                "Card URL text is incorrect.",
                UrlUtilities.stripTrailingSlash(mHoveredTab.getUrl().getSpec()),
                mUrlView.getText());
        verify(mTabHoverCardView).setX(10f);
        verify(mTabHoverCardView).setY(20f);
        verify(mTabHoverCardView).setVisibility(eq(View.VISIBLE));
    }

    @Test
    public void hoveredTabHasMissingThumbnail() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.isIncognito()).thenReturn(false);

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);
        verify(mTabContentManager)
                .getTabThumbnailWithCallback(
                        anyInt(),
                        refEq(new Size(mThumbnailView.getWidth(), mThumbnailView.getHeight())),
                        mGetThumbnailCallbackCaptor.capture());
        mGetThumbnailCallbackCaptor.getValue().onResult(null);
        assertFalse(
                "Thumbnail drawable should not contain a bitmap.",
                mThumbnailView.getDrawable() instanceof BitmapDrawable);
    }

    @Test
    public void hoveredTabChangedBeforeThumbnailCallback() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);
        // Assume that the hovered tab has changed before the thumbnail is fetched.
        when(mHoveredTab.getId()).thenReturn(2);

        verify(mTabContentManager)
                .getTabThumbnailWithCallback(
                        anyInt(),
                        refEq(new Size(mThumbnailView.getWidth(), mThumbnailView.getHeight())),
                        mGetThumbnailCallbackCaptor.capture());
        mGetThumbnailCallbackCaptor.getValue().onResult(mBitmap);
        assertFalse(
                "Thumbnail drawable should not contain a bitmap.",
                mThumbnailView.getDrawable() instanceof BitmapDrawable);
    }

    @Test
    public void hoverCardHiddenBeforeThumbnailCallback() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);
        // Assume that the hover card is hidden before the thumbnail is fetched.
        mTabHoverCardView.hide();
        // Verify state is reset on hide.
        assertFalse("|mIsShowing| should be false.", mTabHoverCardView.isShowingForTesting());
        assertEquals(
                "Hover card view should be hidden.", View.GONE, mTabHoverCardView.getVisibility());
        assertEquals(
                "|mLastHoveredTabId| should be reset.",
                TabHoverCardView.INVALID_TAB_ID,
                mTabHoverCardView.getLastHoveredTabIdForTesting());

        verify(mTabContentManager)
                .getTabThumbnailWithCallback(
                        anyInt(),
                        refEq(new Size(mThumbnailView.getWidth(), mThumbnailView.getHeight())),
                        mGetThumbnailCallbackCaptor.capture());
        mGetThumbnailCallbackCaptor.getValue().onResult(mBitmap);
        assertFalse(
                "Thumbnail drawable should not contain a bitmap.",
                mThumbnailView.getDrawable() instanceof BitmapDrawable);
    }

    @Test
    public void updateHoverCardColors() {
        // Test incognito colors.
        mTabHoverCardView.updateHoverCardColors(true);
        int backgroundColor = R.color.gm3_baseline_surface_container_highest_dark;
        verify(mTabHoverCardView)
                .setBackgroundTintList(
                        eq(
                                ColorStateList.valueOf(
                                        ContextCompat.getColor(mContext, backgroundColor))));
        assertEquals(
                "Title text color is incorrect.",
                mContext.getColor(R.color.default_text_color_light),
                mTitleView.getCurrentTextColor());
        assertEquals(
                "URL text color is incorrect.",
                mContext.getColor(R.color.default_text_color_secondary_light),
                mUrlView.getCurrentTextColor());
        assertEquals(
                "Alert status text color is incorrect.",
                mContext.getColor(R.color.default_text_color_secondary_light),
                mAlertStatusView.getCurrentTextColor());

        // Test standard colors.
        mTabHoverCardView.updateHoverCardColors(false);
        // Invoked in #updateHoverCardColors() in #initialize() in setup and in test.
        verify(mTabHoverCardView, times(2))
                .setBackgroundTintList(
                        eq(
                                ColorStateList.valueOf(
                                        ContextCompat.getColor(
                                                mContext, R.color.tab_hover_card_bg_color))));
        assertEquals(
                "Title text color is incorrect.",
                SemanticColorUtils.getDefaultTextColor(mContext),
                mTitleView.getCurrentTextColor());
        assertEquals(
                "URL text color is incorrect.",
                SemanticColorUtils.getDefaultTextColorSecondary(mContext),
                mUrlView.getCurrentTextColor());
        assertEquals(
                "Alert status text color is incorrect.",
                SemanticColorUtils.getDefaultTextColorSecondary(mContext),
                mAlertStatusView.getCurrentTextColor());
    }

    @Test
    public void initialize() {
        // View is inflated in standard tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        mTabHoverCardView.initialize(mTabModelSelector, mTabContentManagerSupplier);
        // Invoked in #initialize() in setup and in test.
        verify(mTabHoverCardView, times(2)).updateHoverCardColors(false);

        // View is inflated in incognito tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        mTabHoverCardView.initialize(mTabModelSelector, mTabContentManagerSupplier);
        verify(mTabHoverCardView).updateHoverCardColors(true);
    }

    @Test
    public void currentTabModelObserver_OnTabModelSelected() {
        var standardTabModel = mock(TabModel.class);
        var incognitoTabModel = mock(TabModel.class);
        when(standardTabModel.isIncognitoBranded()).thenReturn(false);
        when(incognitoTabModel.isIncognitoBranded()).thenReturn(true);

        // Assume standard tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);

        // Switch to the incognito tab model.
        mTabModelSupplier.set(incognitoTabModel);
        verify(mTabHoverCardView).updateHoverCardColors(true);

        // Switch to the standard tab model.
        mTabModelSupplier.set(standardTabModel);
        // Invoked in #initialize() in setup and in test.
        verify(mTabHoverCardView, times(2)).updateHoverCardColors(false);
    }

    @Test
    public void maybeUpdateBackgroundOnLowEndDevice() {
        SysUtils.setIsLowEndDeviceForTesting(true);
        mTabHoverCardView.maybeUpdateBackgroundOnLowEndDevice();

        assertEquals(
                "Content view background resource is incorrect.",
                R.drawable.popup_bg_8dp,
                shadowOf(mContentView.getBackground()).getCreatedFromResId());
        assertNull("Container background should be null.", mTabHoverCardView.getBackground());
    }

    @Test
    public void testComponentOrder() {
        assertEquals(
                "Component at index 0 should be the title.",
                R.id.title,
                mContentView.getChildAt(0).getId());
        assertEquals(
                "Component at index 1 should be the URL.",
                R.id.url,
                mContentView.getChildAt(1).getId());
        assertEquals(
                "Component at index 2 should be the thumbnail.",
                R.id.thumbnail,
                mContentView.getChildAt(2).getId());
        assertEquals(
                "Component at index 3 should be the alert status.",
                R.id.alert_status,
                mContentView.getChildAt(3).getId());
        assertEquals(
                "Component at index 4 should be the memory usage.",
                R.id.memory_usage,
                mContentView.getChildAt(4).getId());
    }

    @Test
    public void testLiveUpdatesWhileShowing() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);
        when(mHoveredTab.getAlertState()).thenReturn(null);

        mTabHoverCardView.show(mHoveredTab, 10f, 20f);
        verify(mHoveredTab).addObserver(mTabObserverCaptor.capture());
        TabObserver observer = mTabObserverCaptor.getValue();

        assertEquals(
                "Alert status view should be initially hidden.",
                View.GONE,
                mAlertStatusView.getVisibility());

        // Live update alert status.
        observer.onAlertStateChanged(mHoveredTab, TabAlert.GLIC_ACCESSING);
        assertEquals(
                "Alert status view should be visible after update.",
                View.VISIBLE,
                mAlertStatusView.getVisibility());
        assertEquals(
                "Alert status text is incorrect after update.",
                mContext.getString(R.string.tooltip_tab_alert_state_glic_accessing),
                mAlertStatusView.getText().toString());

        // Live update title.
        when(mHoveredTab.getTitle()).thenReturn("Updated Title");
        observer.onTitleUpdated(mHoveredTab);
        assertEquals(
                "Title text should be updated.", "Updated Title", mTitleView.getText().toString());

        // Hide card should remove observer.
        mTabHoverCardView.hide();
        verify(mHoveredTab).removeObserver(observer);
    }
}
