// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
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
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.overlays.strip.StripTabHoverCardViewUnitTest.ShadowSysUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link StripTabHoverCardView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        qualifiers = "sw600dp",
        shadows = {ShadowSysUtils.class})
public class StripTabHoverCardViewUnitTest {
    @Implements(SysUtils.class)
    static class ShadowSysUtils {
        public static boolean sIsLowEndDevice;

        @Implementation
        public static boolean isLowEndDevice() {
            return sIsLowEndDevice;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor private ArgumentCaptor<Callback<Bitmap>> mGetThumbnailCallbackCaptor;

    @Mock private Tab mHoveredTab;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    @Mock private TabContentManager mTabContentManager;
    @Mock private ObservableSupplierImpl<TabModel> mTabModelSupplier;

    private static final float STRIP_STACK_HEIGHT = 500.f;
    private static final float TAB_WIDTH = 100f;

    // Used as a @Spy.
    private StripTabHoverCardView mTabHoverCardView;
    private ViewGroup mContentView;
    private TabThumbnailView mThumbnailView;
    private TextView mTitleView;
    private TextView mUrlView;
    private Context mContext;
    private Bitmap mBitmap;
    private int mHoverCardWidth;

    @Before
    public void setUp() {
        Activity activity = buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        var tabHoverCardView =
                (StripTabHoverCardView)
                        activity.getLayoutInflater().inflate(R.layout.tab_hover_card_holder, null);
        mTabHoverCardView = spy(tabHoverCardView);
        mContentView = mTabHoverCardView.findViewById(R.id.content_view);
        mThumbnailView = mTabHoverCardView.findViewById(R.id.thumbnail);
        mTitleView = mTabHoverCardView.findViewById(R.id.title);
        mUrlView = mTabHoverCardView.findViewById(R.id.url);

        mContext = mTabHoverCardView.getContext();
        mContext.getResources().getDisplayMetrics().density = 1f;

        when(mTabContentManagerSupplier.get()).thenReturn(mTabContentManager);
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

        ShadowSysUtils.sIsLowEndDevice = false;
    }

    @Test
    public void show() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        mTabHoverCardView.show(mHoveredTab, false, 10, 20, STRIP_STACK_HEIGHT);

        assertEquals("Card title text is incorrect.", mHoveredTab.getTitle(), mTitleView.getText());
        assertEquals(
                "Card URL text is incorrect.", mHoveredTab.getUrl().getHost(), mUrlView.getText());
        assertEquals(
                "|mLastHoveredTabId| is incorrect.",
                1,
                mTabHoverCardView.getLastHoveredTabIdForTesting());
        assertTrue("|mIsShowing| should be true.", mTabHoverCardView.isShowingForTesting());
        verify(mTabHoverCardView).setX(anyFloat());
        verify(mTabHoverCardView).setY(anyFloat());
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
    public void hoveredTabUsesChromeScheme() {
        var url = JUnitTestGURLs.NTP_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);

        mTabHoverCardView.show(mHoveredTab, false, 10, 20, STRIP_STACK_HEIGHT);

        assertEquals("Card title text is incorrect.", mHoveredTab.getTitle(), mTitleView.getText());
        // Verify chrome:// tab hover card display text.
        assertEquals(
                "Card URL text is incorrect.",
                mHoveredTab.getUrl().getSpec().replaceFirst("/$", ""),
                mUrlView.getText());
        verify(mTabHoverCardView).setX(anyFloat());
        verify(mTabHoverCardView).setY(anyFloat());
        verify(mTabHoverCardView).setVisibility(eq(View.VISIBLE));
    }

    @Test
    public void hoveredTabHasMissingThumbnail() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.isIncognito()).thenReturn(false);

        mTabHoverCardView.show(mHoveredTab, false, 10, 20, STRIP_STACK_HEIGHT);
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

        mTabHoverCardView.show(mHoveredTab, false, 10, 20, STRIP_STACK_HEIGHT);
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

        mTabHoverCardView.show(mHoveredTab, false, 10, 20, STRIP_STACK_HEIGHT);
        // Assume that the hover card is hidden before the thumbnail is fetched.
        mTabHoverCardView.hide();
        // Verify state is reset on hide.
        assertFalse("|mIsShowing| should be false.", mTabHoverCardView.isShowingForTesting());
        assertEquals(
                "Hover card view should be hidden.", View.GONE, mTabHoverCardView.getVisibility());
        assertEquals(
                "|mLastHoveredTabId| should be reset.",
                StripTabHoverCardView.INVALID_TAB_ID,
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
    public void getHoverCardPosition() {
        // Set simulated hovered tab drawX for expected hover card position.
        float[] position = mTabHoverCardView.getHoverCardPosition(false, 10, 0, STRIP_STACK_HEIGHT);
        float inactiveTabCardXOffset =
                mContext.getResources().getDimension(R.dimen.inactive_tab_hover_card_x_offset);
        assertEquals(
                "Card x position is incorrect.", 10f + inactiveTabCardXOffset, position[0], 0f);
        assertEquals("Card y position is incorrect.", STRIP_STACK_HEIGHT, position[1], 0f);
    }

    @Test
    public void getHoverCardPosition_CardWidthExceedsWindowWidth() {
        // Set window width to be slightly smaller than the default card width.
        mContext.getResources().getDisplayMetrics().widthPixels = (int) (mHoverCardWidth - 1);

        // Set simulated hovered tab drawX for expected hover card position.
        float[] position =
                mTabHoverCardView.getHoverCardPosition(true, 10f, TAB_WIDTH, STRIP_STACK_HEIGHT);
        ArgumentCaptor<LayoutParams> captor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mTabHoverCardView).setLayoutParams(captor.capture());
        assertEquals(
                "Card width is incorrect.",
                Math.round(0.9f * (mHoverCardWidth - 1)),
                captor.getValue().width);
        assertEquals("Card x position is incorrect.", 10f, position[0], 0f);
        assertEquals("Card y position is incorrect.", STRIP_STACK_HEIGHT, position[1], 0f);
    }

    @Test
    public void cardWidthAcrossWindowResizes() {
        // Set window width to be slightly smaller than the default card width.
        mContext.getResources().getDisplayMetrics().widthPixels = (int) (mHoverCardWidth - 1);
        mTabHoverCardView.getHoverCardPosition(false, 10f, TAB_WIDTH, STRIP_STACK_HEIGHT);

        // Set window width to be big enough to accommodate the default card width.
        mContext.getResources().getDisplayMetrics().widthPixels = (int) (mHoverCardWidth * 2);
        // Last LayoutParams should reflect updated width.
        when(mTabHoverCardView.getLayoutParams())
                .thenReturn(new LayoutParams(Math.round(0.9f * (mHoverCardWidth - 1)), 200));
        mTabHoverCardView.getHoverCardPosition(false, 10f, TAB_WIDTH, STRIP_STACK_HEIGHT);

        ArgumentCaptor<LayoutParams> captor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(mTabHoverCardView, times(2)).setLayoutParams(captor.capture());
        var paramsList = captor.getAllValues();
        assertEquals(
                "Card width within small window is incorrect.",
                Math.round(0.9f * (mHoverCardWidth - 1)),
                paramsList.get(0).width);
        assertEquals(
                "Card width within big window is incorrect.",
                mHoverCardWidth,
                paramsList.get(1).width);
    }

    @Test
    public void getHoverCardPosition_CardCrossesWindowBounds() {
        float windowHorizontalMargin =
                mContext.getResources()
                        .getDimension(R.dimen.tab_hover_card_window_horizontal_margin);

        // Assume that the tab's hover card is positioned beyond the left edge of the app window.
        float[] position =
                mTabHoverCardView.getHoverCardPosition(true, -1f, TAB_WIDTH, STRIP_STACK_HEIGHT);
        assertEquals(
                "Card should maintain a minimum margin from the left edge of the app window.",
                windowHorizontalMargin,
                position[0],
                0f);

        // Assume that the tab's hover card extends beyond the right edge of the app window.
        int windowWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        position =
                mTabHoverCardView.getHoverCardPosition(
                        true, windowWidth - mHoverCardWidth + 1f, TAB_WIDTH, STRIP_STACK_HEIGHT);
        assertEquals(
                "Card should maintain a minimum margin from the right edge of the app window.",
                windowWidth - mHoverCardWidth - windowHorizontalMargin,
                position[0],
                0f);
    }

    @Test
    public void getHoverCardPosition_RtlLayout() {
        LocalizationUtils.setRtlForTesting(true);

        // Set simulated hovered tab drawX and width for expected hover card position.
        float[] position =
                mTabHoverCardView.getHoverCardPosition(
                        false, 28, mHoverCardWidth - 2f, STRIP_STACK_HEIGHT);
        float detachedCardOffset =
                mContext.getResources().getDimension(R.dimen.inactive_tab_hover_card_x_offset);
        assertEquals("Card x position is incorrect.", 26f - detachedCardOffset, position[0], 0f);
    }

    @Test
    public void getHoverCardPosition_LowEndDevice() {
        ShadowSysUtils.sIsLowEndDevice = true;

        float[] position =
                mTabHoverCardView.getHoverCardPosition(false, 10f, TAB_WIDTH, STRIP_STACK_HEIGHT);
        float detachedCardOffset =
                mContext.getResources().getDimension(R.dimen.inactive_tab_hover_card_x_offset);
        float cardShadowLength =
                mContext.getResources().getDimension(R.dimen.tab_hover_card_elevation);
        assertEquals(
                "Card x position is incorrect.",
                10f + detachedCardOffset - cardShadowLength,
                position[0],
                0f);
        assertEquals(
                "Card y position is incorrect.",
                STRIP_STACK_HEIGHT
                        - cardShadowLength,
                position[1],
                0f);
    }

    @Test
    public void updateHoverCardColors() {
        // Test incognito colors.
        mTabHoverCardView.updateHoverCardColors(true);
        verify(mTabHoverCardView)
                .setBackgroundTintList(
                        eq(
                                ColorStateList.valueOf(
                                        ContextCompat.getColor(
                                                mContext,
                                                R.color.default_bg_color_dark_elev_5_baseline))));
        assertEquals(
                "Title text color is incorrect.",
                mContext.getColor(R.color.default_text_color_light),
                mTitleView.getCurrentTextColor());
        assertEquals(
                "URL text color is incorrect.",
                mContext.getColor(R.color.default_text_color_secondary_light),
                mUrlView.getCurrentTextColor());

        // Test standard colors.
        mTabHoverCardView.updateHoverCardColors(false);
        // Invoked in #updateHoverCardColors() in #initialize() in setup and in test.
        verify(mTabHoverCardView, times(2))
                .setBackgroundTintList(
                        eq(
                                ColorStateList.valueOf(
                                        ChromeColors.getSurfaceColor(
                                                mContext, R.dimen.tab_hover_card_bg_color_elev))));
        assertEquals(
                "Title text color is incorrect.",
                SemanticColorUtils.getDefaultTextColor(mContext),
                mTitleView.getCurrentTextColor());
        assertEquals(
                "URL text color is incorrect.",
                SemanticColorUtils.getDefaultTextColorSecondary(mContext),
                mUrlView.getCurrentTextColor());
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
        // TabModelObserver should be added after the view is inflated.
        mTabHoverCardView.initialize(mTabModelSelector, mTabContentManagerSupplier);
        var tabModelObserver = mTabHoverCardView.getCurrentTabModelObserverForTesting();
        assertNotNull("TabModelSelectorObserver should be set.", tabModelObserver);

        // Switch to the incognito tab model.
        tabModelObserver.onResult(incognitoTabModel);
        verify(mTabHoverCardView).updateHoverCardColors(true);

        // Switch to the standard tab model.
        tabModelObserver.onResult(standardTabModel);
        // Invoked in #initialize() in setup and in test, and in #onTabModelSelected().
        verify(mTabHoverCardView, times(3)).updateHoverCardColors(false);
    }

    @Test
    public void maybeUpdateBackgroundOnLowEndDevice() {
        ShadowSysUtils.sIsLowEndDevice = true;
        mTabHoverCardView.maybeUpdateBackgroundOnLowEndDevice();

        assertEquals(
                "Content view background resource is incorrect.",
                R.drawable.popup_bg_8dp,
                shadowOf(mContentView.getBackground()).getCreatedFromResId());
        assertNull("Container background should be null.", mTabHoverCardView.getBackground());
    }
}
