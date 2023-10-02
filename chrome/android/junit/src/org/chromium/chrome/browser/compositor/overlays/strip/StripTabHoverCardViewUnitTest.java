// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.util.Size;
import android.view.View;
import android.widget.ImageView.ScaleType;
import android.widget.TextView;

import androidx.coordinatorlayout.widget.CoordinatorLayout.LayoutParams;
import androidx.core.content.ContextCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabGridThumbnailView;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link StripTabHoverCardView}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN,
        ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP})
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripTabHoverCardViewUnitTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor
    private ArgumentCaptor<Callback<Bitmap>> mGetThumbnailCallbackCaptor;

    @Mock
    private Tab mHoveredTab;
    @Mock
    private StripLayoutTab mHoveredStripTab;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    @Mock
    private TabContentManager mTabContentManager;

    private static final float STRIP_STACK_HEIGHT = 500.f;

    private StripTabHoverCardView mTabHoverCardView;
    private TabGridThumbnailView mThumbnailView;
    private TextView mTitleView;
    private TextView mUrlView;
    private Context mContext;
    private Bitmap mBitmap;
    private int mHoverCardWidth;

    @Before
    public void setUp() {
        Activity activity = buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mTabHoverCardView = (StripTabHoverCardView) activity.getLayoutInflater().inflate(
                R.layout.tab_hover_card_holder, null);
        mThumbnailView = mTabHoverCardView.findViewById(R.id.thumbnail);
        mTitleView = mTabHoverCardView.findViewById(R.id.title);
        mUrlView = mTabHoverCardView.findViewById(R.id.url);

        mContext = mTabHoverCardView.getContext();
        mContext.getResources().getDisplayMetrics().density = 1f;

        when(mTabContentManagerSupplier.get()).thenReturn(mTabContentManager);
        mTabHoverCardView.initialize(mTabModelSelector, mTabContentManagerSupplier);
        mBitmap = Bitmap.createBitmap(100, 200, Bitmap.Config.RGB_565);

        mHoverCardWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.tab_hover_card_width);
        int thumbnailHeight = mContext.getResources().getDimensionPixelSize(
                R.dimen.tab_hover_card_thumbnail_height);
        mThumbnailView.measure(mHoverCardWidth, thumbnailHeight);
        mThumbnailView.layout(0, 0, mHoverCardWidth, thumbnailHeight);
    }

    @Test
    public void show() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);
        cardViewSpy.show(mHoveredTab, mHoveredStripTab, false, STRIP_STACK_HEIGHT);

        assertEquals("Card title text is incorrect.", mHoveredTab.getTitle(), mTitleView.getText());
        assertEquals(
                "Card URL text is incorrect.", mHoveredTab.getUrl().getHost(), mUrlView.getText());
        assertEquals("|mLastHoveredTabId| is incorrect.", 1,
                cardViewSpy.getLastHoveredTabIdForTesting());
        assertTrue("|mIsShowing| should be true.", cardViewSpy.isShowingForTesting());
        verify(cardViewSpy).setX(anyFloat());
        verify(cardViewSpy).setY(anyFloat());
        verify(cardViewSpy).setVisibility(eq(View.VISIBLE));

        verify(mTabContentManager)
                .getTabThumbnailWithCallback(anyInt(),
                        refEq(new Size(mThumbnailView.getWidth(), mThumbnailView.getHeight())),
                        mGetThumbnailCallbackCaptor.capture(), eq(false), eq(false));
        mGetThumbnailCallbackCaptor.getValue().onResult(mBitmap);

        assertEquals("Thumbnail scale type is incorrect.", ScaleType.MATRIX,
                mThumbnailView.getScaleType());
        assertNotNull("Thumbnail image matrix should be set.", mThumbnailView.getImageMatrix());
        assertEquals("Thumbnail image bitmap is incorrect.", mBitmap,
                ((BitmapDrawable) mThumbnailView.getDrawable()).getBitmap());
    }

    @Test
    public void hoveredTabUsesChromeScheme() {
        var url = JUnitTestGURLs.NTP_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);

        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);
        cardViewSpy.show(mHoveredTab, mHoveredStripTab, false, STRIP_STACK_HEIGHT);

        assertEquals("Card title text is incorrect.", mHoveredTab.getTitle(), mTitleView.getText());
        // Verify chrome:// tab hover card display text.
        assertEquals("Card URL text is incorrect.",
                mHoveredTab.getUrl().getSpec().replaceFirst("/$", ""), mUrlView.getText());
        verify(cardViewSpy).setX(anyFloat());
        verify(cardViewSpy).setY(anyFloat());
        verify(cardViewSpy).setVisibility(eq(View.VISIBLE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.THUMBNAIL_PLACEHOLDER)
    public void hoveredTabHasMissingThumbnail() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.isIncognito()).thenReturn(false);

        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);
        cardViewSpy.show(mHoveredTab, mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        verify(mTabContentManager)
                .getTabThumbnailWithCallback(anyInt(),
                        refEq(new Size(mThumbnailView.getWidth(), mThumbnailView.getHeight())),
                        mGetThumbnailCallbackCaptor.capture(), eq(false), eq(false));
        mGetThumbnailCallbackCaptor.getValue().onResult(null);
        assertFalse("Thumbnail drawable should not contain a bitmap.",
                mThumbnailView.getDrawable() instanceof BitmapDrawable);
    }

    @Test
    public void hoveredTabChangedBeforeThumbnailCallback() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);
        when(mHoveredTab.getId()).thenReturn(1);

        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);
        cardViewSpy.show(mHoveredTab, mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        // Assume that the hovered tab has changed before the thumbnail is fetched.
        when(mHoveredTab.getId()).thenReturn(2);

        verify(mTabContentManager)
                .getTabThumbnailWithCallback(anyInt(),
                        refEq(new Size(mThumbnailView.getWidth(), mThumbnailView.getHeight())),
                        mGetThumbnailCallbackCaptor.capture(), eq(false), eq(false));
        mGetThumbnailCallbackCaptor.getValue().onResult(mBitmap);
        assertFalse("Thumbnail drawable should not contain a bitmap.",
                mThumbnailView.getDrawable() instanceof BitmapDrawable);
    }

    @Test
    public void hoverCardHiddenBeforeThumbnailCallback() {
        var url = JUnitTestGURLs.EXAMPLE_URL;
        var title = "Tab 1";
        when(mHoveredTab.getTitle()).thenReturn(title);
        when(mHoveredTab.getUrl()).thenReturn(url);

        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);
        cardViewSpy.show(mHoveredTab, mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        // Assume that the hover card is hidden before the thumbnail is fetched.
        cardViewSpy.hide();
        // Verify state is reset on hide.
        assertFalse("|mIsShowing| should be false.", mTabHoverCardView.isShowingForTesting());
        assertEquals(
                "Hover card view should be hidden.", View.GONE, mTabHoverCardView.getVisibility());
        assertEquals("|mLastHoveredTabId| should be reset.", StripTabHoverCardView.INVALID_TAB_ID,
                mTabHoverCardView.getLastHoveredTabIdForTesting());

        verify(mTabContentManager)
                .getTabThumbnailWithCallback(anyInt(),
                        refEq(new Size(mThumbnailView.getWidth(), mThumbnailView.getHeight())),
                        mGetThumbnailCallbackCaptor.capture(), eq(false), eq(false));
        mGetThumbnailCallbackCaptor.getValue().onResult(mBitmap);
        assertFalse("Thumbnail drawable should not contain a bitmap.",
                mThumbnailView.getDrawable() instanceof BitmapDrawable);
    }

    @Test
    public void getHoverCardPosition() {
        // Use TSR detached treatment for additional coverage that includes position adjustments.
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        // Set simulated hovered StripLayoutTab drawX for expected hover card position.
        when(mHoveredStripTab.getDrawX()).thenReturn(10f);

        float[] position =
                mTabHoverCardView.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        float detachedCardOffset =
                mContext.getResources().getDimension(R.dimen.tsr_no_feet_tab_hover_card_x_offset);
        assertEquals("Card x position is incorrect.", 10f + detachedCardOffset, position[0], 0f);
        assertEquals("Card y position is incorrect.",
                STRIP_STACK_HEIGHT + StripLayoutHelper.FOLIO_DETACHED_BOTTOM_MARGIN_DP, position[1],
                0f);
    }

    @Test
    public void getHoverCardPosition_CardWidthExceedsWindowWidth() {
        // Set window width to be slightly smaller than the default card width.
        mContext.getResources().getDisplayMetrics().widthPixels = (int) (mHoverCardWidth - 1);
        // Set simulated hovered StripLayoutTab drawX for expected hover card position.
        when(mHoveredStripTab.getDrawX()).thenReturn(10f);
        var originalLayoutParams = new LayoutParams((int) mHoverCardWidth, 200);
        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);
        when(cardViewSpy.getLayoutParams()).thenReturn(originalLayoutParams);

        float[] position =
                cardViewSpy.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        ArgumentCaptor<LayoutParams> captor = ArgumentCaptor.forClass(LayoutParams.class);
        verify(cardViewSpy).setLayoutParams(captor.capture());
        assertEquals("Card width is incorrect.", Math.round(0.9f * (mHoverCardWidth - 1)),
                captor.getValue().width);
        assertEquals("Card x position is incorrect.", 10f, position[0], 0f);
        assertEquals("Card y position is incorrect.", STRIP_STACK_HEIGHT, position[1], 0f);
    }

    @Test
    public void getHoverCardPosition_CardCrossesWindowBounds() {
        float windowHorizontalMargin = mContext.getResources().getDimension(
                R.dimen.tab_hover_card_window_horizontal_margin);

        // Assume that the tab's hover card is positioned beyond the left edge of the app window.
        when(mHoveredStripTab.getDrawX()).thenReturn(-1f);
        float[] position =
                mTabHoverCardView.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        assertEquals("Card should maintain a minimum margin from the left edge of the app window.",
                windowHorizontalMargin, position[0], 0f);

        // Assume that the tab's hover card extends beyond the right edge of the app window.
        int windowWidth = mContext.getResources().getDisplayMetrics().widthPixels;
        when(mHoveredStripTab.getDrawX()).thenReturn(windowWidth - mHoverCardWidth + 1f);
        position =
                mTabHoverCardView.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        assertEquals("Card should maintain a minimum margin from the right edge of the app window.",
                windowWidth - mHoverCardWidth - windowHorizontalMargin, position[0], 0f);
    }

    @Test
    public void getHoverCardPosition_RtlLayout() {
        // Use TSR detached treatment for additional coverage that includes position adjustments.
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        LocalizationUtils.setRtlForTesting(true);

        // Set simulated hovered StripLayoutTab drawX and width for expected hover card position.
        when(mHoveredStripTab.getDrawX()).thenReturn(28f);
        when(mHoveredStripTab.getWidth()).thenReturn(mHoverCardWidth - 2f);

        float[] position =
                mTabHoverCardView.getHoverCardPosition(mHoveredStripTab, false, STRIP_STACK_HEIGHT);
        float detachedCardOffset =
                mContext.getResources().getDimension(R.dimen.tsr_no_feet_tab_hover_card_x_offset);
        assertEquals("Card x position is incorrect.", 26f - detachedCardOffset, position[0], 0f);
    }

    @Test
    public void updateHoverCardColors() {
        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);

        // Test incognito colors.
        cardViewSpy.updateHoverCardColors(true);
        verify(cardViewSpy)
                .setBackgroundTintList(eq(ColorStateList.valueOf(ContextCompat.getColor(
                        mContext, R.color.default_bg_color_dark_elev_5_baseline))));
        assertEquals("Title text color is incorrect.",
                mContext.getColor(R.color.default_text_color_light),
                mTitleView.getCurrentTextColor());
        assertEquals("URL text color is incorrect.",
                mContext.getColor(R.color.default_text_color_secondary_light),
                mUrlView.getCurrentTextColor());

        // Test standard colors.
        cardViewSpy.updateHoverCardColors(false);
        verify(cardViewSpy)
                .setBackgroundTintList(eq(ColorStateList.valueOf(ChromeColors.getSurfaceColor(
                        mContext, R.dimen.tab_hover_card_bg_color_elev))));
        assertEquals("Title text color is incorrect.",
                SemanticColorUtils.getDefaultTextColor(mContext), mTitleView.getCurrentTextColor());
        assertEquals("URL text color is incorrect.",
                SemanticColorUtils.getDefaultTextColorSecondary(mContext),
                mUrlView.getCurrentTextColor());
    }

    @Test
    public void initialize() {
        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);

        // View is inflated in standard tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        cardViewSpy.initialize(mTabModelSelector, mTabContentManagerSupplier);
        verify(cardViewSpy).updateHoverCardColors(false);

        // View is inflated in incognito tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);
        cardViewSpy.initialize(mTabModelSelector, mTabContentManagerSupplier);
        verify(cardViewSpy).updateHoverCardColors(true);
    }

    @Test
    public void tabModelSelectorObserver_OnTabModelSelected() {
        var standardTabModel = mock(TabModel.class);
        var incognitoTabModel = mock(TabModel.class);
        when(standardTabModel.isIncognito()).thenReturn(false);
        when(incognitoTabModel.isIncognito()).thenReturn(true);

        StripTabHoverCardView cardViewSpy = spy(mTabHoverCardView);

        // Assume standard tab model.
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(false);
        // TabModelSelectorObserver should be added after the view is inflated.
        cardViewSpy.initialize(mTabModelSelector, mTabContentManagerSupplier);
        var tabModelSelectorObserver = cardViewSpy.getTabModelSelectorObserverForTesting();
        assertNotNull("TabModelSelectorObserver should be set.", tabModelSelectorObserver);

        // Switch to the incognito tab model.
        tabModelSelectorObserver.onTabModelSelected(incognitoTabModel, standardTabModel);
        verify(cardViewSpy).updateHoverCardColors(true);

        // Switch to the standard tab model.
        tabModelSelectorObserver.onTabModelSelected(standardTabModel, incognitoTabModel);
        // Invoked once in #onInflate(), subsequently in #onTabModelSelected().
        verify(cardViewSpy, times(2)).updateHoverCardColors(false);
    }
}
