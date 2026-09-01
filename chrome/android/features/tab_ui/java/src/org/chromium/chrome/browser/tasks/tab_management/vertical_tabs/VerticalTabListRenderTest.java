// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.TabStripContextMenuCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabHoverCardView;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tabs.TabAlert;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.List;

// TODO(crbug.com/521987032): Add tests for nested children with actor indicator.
// TODO(crbug.com/509226293): Add tests for RTL layout.

/** Render tests for Vertical Tabs UI (TabVerticalViewBinder). */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class VerticalTabListRenderTest {
    private static final int COLLAPSED_RAIL_WIDTH_DP =
            VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP;
    private static final int EXPANDED_RAIL_WIDTH_DP = 206;
    private static final int RAIL_TEST_HEIGHT_DP = 400;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            List.of(
                    new ParameterSet()
                            .value(false, false)
                            .name("NightModeDisabled_IncognitoDisabled"),
                    new ParameterSet()
                            .value(true, false)
                            .name("NightModeEnabled_IncognitoDisabled"),
                    new ParameterSet().value(true, true).name("NightModeEnabled_IncognitoEnabled"));

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(2)
                    .build();

    private final boolean mIsIncognito;
    private Activity mActivity;
    private FrameLayout mRenderView;
    private int mPinnedItemWidthPx;
    private int mOriginalSmallestScreenWidthDp;

    public VerticalTabListRenderTest(boolean isNightModeEnabled, boolean isIncognito) {
        mIsIncognito = isIncognito;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(isNightModeEnabled);
        mRenderTestRule.setNightModeEnabled(isNightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(
                mIsIncognito
                        ? R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito
                        : R.style.Theme_BrowserUI_DayNight);
        mPinnedItemWidthPx =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.vertical_tab_pinned_item_min_width);

        mOriginalSmallestScreenWidthDp =
                mActivity.getResources().getConfiguration().smallestScreenWidthDp;
    }

    @After
    public void tearDown() {
        // Reset smallestScreenWidthDp.
        if (mOriginalSmallestScreenWidthDp != 0 && mActivity != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        Configuration config = mActivity.getResources().getConfiguration();
                        config.smallestScreenWidthDp = mOriginalSmallestScreenWidthDp;
                        mActivity
                                .getResources()
                                .updateConfiguration(
                                        config, mActivity.getResources().getDisplayMetrics());
                    });
        }
    }

    private ViewGroup inflateAndAttachView(int layoutResId) {
        return inflateAndAttachView(layoutResId, ViewGroup.LayoutParams.WRAP_CONTENT);
    }

    private ViewGroup inflateAndAttachView(int layoutResId, int contentWidthPx) {
        FrameLayout parent = new FrameLayout(mActivity);
        mActivity.setContentView(
                parent,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mRenderView = new FrameLayout(mActivity);

        Context themeContext = new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        mRenderView.setBackgroundColor(
                SemanticColorUtils.getColorSurfaceContainerHighest(themeContext));

        int padding = ViewUtils.dpToPx(mActivity, 8);
        mRenderView.setPadding(padding, padding, padding, padding);

        ViewGroup view = inflateView(layoutResId, mRenderView);
        mRenderView.addView(view);
        int width =
                contentWidthPx == ViewGroup.LayoutParams.WRAP_CONTENT
                        ? ViewGroup.LayoutParams.WRAP_CONTENT
                        : contentWidthPx + 2 * padding;

        parent.addView(
                mRenderView,
                new FrameLayout.LayoutParams(width, ViewGroup.LayoutParams.WRAP_CONTENT));

        return view;
    }

    private ViewGroup inflateView(int layoutResId, @Nullable ViewGroup root) {
        return (ViewGroup)
                LayoutInflater.from(mActivity)
                        .inflate(layoutResId, root, /* attachToRoot= */ false);
    }

    private TabFaviconFetcher createFaviconFetcher() {
        return callback -> {
            Drawable drawable =
                    AppCompatResources.getDrawable(
                            mActivity,
                            mIsIncognito ? R.drawable.ic_incognito_24dp : R.drawable.ic_globe_24dp);
            callback.onResult(
                    new TabFavicon(drawable, drawable, false) {
                        @Override
                        public boolean equals(Object other) {
                            return false;
                        }
                    });
        };
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Unselected() throws IOException {
        testStandardTab(
                "Standard Tab",
                /* isSelected= */ false,
                /* isLoading= */ false,
                /* isHovered= */ false,
                "standard_tab_unselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Active() throws IOException {
        testStandardTab(
                "Active Tab",
                /* isSelected= */ true,
                /* isLoading= */ false,
                /* isHovered= */ false,
                "standard_tab_active");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Loading() throws IOException {
        testStandardTab(
                "Loading Tab",
                /* isSelected= */ false,
                /* isLoading= */ true,
                /* isHovered= */ false,
                "standard_tab_loading");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_ActorIndicator_Dynamic() throws IOException {
        if (mIsIncognito) return;
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false);
                    PropertyModel model =
                            createTabListItemModelBuilder("AI Tab", /* groupId= */ null)
                                    .with(TabProperties.ACTOR_UI_STATE, uiTabState)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_actor_indicator_dynamic");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_ActorIndicator_Static() throws IOException {
        if (mIsIncognito) return;
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.STATIC, false);
                    PropertyModel model =
                            createTabListItemModelBuilder("AI Tab", /* groupId= */ null)
                                    .with(TabProperties.ACTOR_UI_STATE, uiTabState)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_actor_indicator_static");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_GlicIndicator_Active() throws IOException {
        if (mIsIncognito) return;
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            createTabListItemModelBuilder("Glic Tab", /* groupId= */ null)
                                    .with(TabProperties.IS_GLIC_ACTIVE, true)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_glic_indicator_active");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testChildTab_GlicIndicator_Active() throws IOException {
        if (mIsIncognito) return;
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            createTabListItemModelBuilder(
                                            "Child Glic Tab", /* groupId= */ Token.createRandom())
                                    .with(TabProperties.IS_GLIC_ACTIVE, true)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "child_tab_glic_indicator_active");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Active_ActorIndicator_Dynamic() throws IOException {
        if (mIsIncognito) return;
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false);
                    PropertyModel model =
                            createTabListItemModelBuilder("Active AI Tab", /* groupId= */ null)
                                    .with(TabProperties.IS_SELECTED, true)
                                    .with(TabProperties.ACTOR_UI_STATE, uiTabState)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_active_actor_indicator_dynamic");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Active_ActorIndicator_Static() throws IOException {
        if (mIsIncognito) return;
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.STATIC, false);
                    PropertyModel model =
                            createTabListItemModelBuilder("Active AI Tab", /* groupId= */ null)
                                    .with(TabProperties.IS_SELECTED, true)
                                    .with(TabProperties.ACTOR_UI_STATE, uiTabState)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_active_actor_indicator_static");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_MediaIndicator() throws IOException {
        if (mIsIncognito) return;
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            createTabListItemModelBuilder("Media Tab", /* groupId= */ null)
                                    .with(TabProperties.MEDIA_INDICATOR, MediaState.AUDIBLE)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE, null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_media_indicator");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Hovered() throws IOException {
        testStandardTab(
                "Hovered Tab",
                /* isSelected= */ false,
                /* isLoading= */ false,
                /* isHovered= */ true,
                "standard_tab_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_Unselected() throws IOException {
        testPinnedTab(
                "Pinned Tab",
                /* isSelected= */ false,
                /* isLoading= */ false,
                /* isHovered= */ false,
                "pinned_tab_unselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_Active() throws IOException {
        testPinnedTab(
                "Pinned Tab",
                /* isSelected= */ true,
                /* isLoading= */ false,
                /* isHovered= */ false,
                "pinned_tab_active");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_Loading() throws IOException {
        testPinnedTab(
                "Loading Pinned Tab",
                /* isSelected= */ false,
                /* isLoading= */ true,
                /* isHovered= */ false,
                "pinned_tab_loading");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_Hovered() throws IOException {
        testPinnedTab(
                "Hovered Pinned Tab",
                /* isSelected= */ false,
                /* isLoading= */ false,
                /* isHovered= */ true,
                "pinned_tab_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_GlicIndicator_Active() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] =
                            inflateAndAttachView(
                                    R.layout.vertical_tab_pinned_item, mPinnedItemWidthPx);
                    PropertyModel model =
                            createTabListItemModelBuilder(
                                            (mIsIncognito ? "Incognito " : "") + "Pinned Tab",
                                            /* groupId= */ null)
                                    .with(TabProperties.IS_PINNED, true)
                                    .with(TabProperties.IS_GLIC_ACTIVE, true)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindPinnedTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        String finalGoldenName =
                mIsIncognito
                        ? "pinned_tab_glic_indicator_active_incognito"
                        : "pinned_tab_glic_indicator_active";
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Collapsed() throws IOException {
        testTabGroupHeader(
                "Collapsed Group",
                /* isCollapsed= */ true,
                /* isHovered= */ false,
                "tab_group_header_collapsed");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Expanded() throws IOException {
        testTabGroupHeader(
                "Expanded Group",
                /* isCollapsed= */ false,
                /* isHovered= */ false,
                "tab_group_header_expanded");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Collapsed_Hovered() throws IOException {
        testTabGroupHeader(
                "Hovered Group (Collapsed)",
                /* isCollapsed= */ true,
                /* isHovered= */ true,
                "tab_group_header_collapsed_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Expanded_Hovered() throws IOException {
        testTabGroupHeader(
                "Hovered Group (Expanded)",
                /* isCollapsed= */ false,
                /* isHovered= */ true,
                "tab_group_header_expanded_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Expanded() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ false, /* isRtl= */ false, /* isHeaderOffScreen= */ false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Expanded_Rtl() throws IOException {
        if (mIsIncognito) return;
        LocalizationUtils.setRtlForTesting(true);
        try {
            testTabGroupSpine(
                    /* isCollapsed= */ false, /* isRtl= */ true, /* isHeaderOffScreen= */ false);
        } finally {
            LocalizationUtils.setRtlForTesting(false);
        }
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Collapsed() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ true, /* isRtl= */ false, /* isHeaderOffScreen= */ false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_HeaderOffScreen() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ false, /* isRtl= */ false, /* isHeaderOffScreen= */ true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testHeaderContainer() throws IOException {
        if (mIsIncognito) return;
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    VerticalTabRailLayout container =
                            (VerticalTabRailLayout)
                                    inflateAndAttachView(R.layout.vertical_tab_layout);
                    int widthPx = ViewUtils.dpToPx(mActivity, EXPANDED_RAIL_WIDTH_DP);
                    container.setLayoutParams(
                            new FrameLayout.LayoutParams(
                                    widthPx, ViewGroup.LayoutParams.WRAP_CONTENT));

                    PropertyModel model =
                            new PropertyModel.Builder(VerticalTabListProperties.ALL_KEYS).build();
                    PropertyModelChangeProcessor.create(
                            model, container, VerticalTabListViewBinder::bind);

                    view[0] = container.findViewById(R.id.vertical_tab_header_container);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(view[0], "vertical_tab_header_expanded");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testCollapsedRail() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    VerticalTabRailLayout container =
                            (VerticalTabRailLayout)
                                    inflateAndAttachView(R.layout.vertical_tab_layout);
                    int widthPx = ViewUtils.dpToPx(mActivity, COLLAPSED_RAIL_WIDTH_DP);
                    int heightPx = ViewUtils.dpToPx(mActivity, RAIL_TEST_HEIGHT_DP);
                    container.setLayoutParams(new FrameLayout.LayoutParams(widthPx, heightPx));

                    PropertyModel containerModel =
                            new PropertyModel.Builder(VerticalTabListProperties.ALL_KEYS)
                                    .with(
                                            VerticalTabListProperties.COLLAPSE_STATE,
                                            RailCollapseState.COLLAPSED)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            containerModel, container, VerticalTabListViewBinder::bind);

                    // Setup Pinned Tabs Recycler View.
                    TabListRecyclerView pinnedRecyclerView =
                            container.findViewById(R.id.pinned_tabs_recycler_view);
                    pinnedRecyclerView.setVisibility(View.VISIBLE);
                    pinnedRecyclerView.setLayoutManager(new GridLayoutManager(mActivity, 1));
                    TabListModel pinnedTabsModel = new TabListModel();
                    pinnedRecyclerView.setAdapter(createPinnedTabListAdapter(pinnedTabsModel));

                    PropertyModel pinnedTabModel =
                            createTabListItemModelBuilder("Pinned Tab", /* groupId= */ null)
                                    .with(TabProperties.IS_PINNED, true)
                                    .with(
                                            TabProperties.RAIL_COLLAPSE_STATE,
                                            RailCollapseState.COLLAPSED)
                                    .build();
                    addPinnedTabListItem(pinnedTabsModel, pinnedTabModel);

                    // Setup Tab List Recycler View.
                    TabListRecyclerView recyclerView =
                            container.findViewById(R.id.tab_list_recycler_view);
                    recyclerView.setVisibility(View.VISIBLE);
                    recyclerView.setLayoutManager(new LinearLayoutManager(mActivity));

                    TabListModel tabListModel = new TabListModel();
                    TabModel tabModel = mock(TabModel.class);
                    when(tabModel.isIncognitoBranded()).thenReturn(mIsIncognito);
                    TabModelSelector tabModelSelector = setupMockTabModelSelector(tabModel);

                    recyclerView.addItemDecoration(
                            new VerticalTabGroupSpineDecoration(
                                    mActivity, () -> {}, tabListModel, tabModelSelector));
                    recyclerView.setAdapter(createTabListAdapter(tabListModel));

                    // Normal Tab
                    addTabListItem(
                            tabListModel,
                            createTabListItemModelBuilder("Normal Tab", /* groupId= */ null)
                                    .with(
                                            TabProperties.RAIL_COLLAPSE_STATE,
                                            RailCollapseState.COLLAPSED)
                                    .build());

                    // Tab Group Header and Nesting Tabs
                    Token groupId = Token.createRandom();
                    when(tabModel.getTabGroupColorWithFallback(groupId))
                            .thenReturn(TabGroupColorId.BLUE);

                    addGroupHeaderListItem(
                            tabListModel,
                            createGroupHeaderItemModelBuilder(
                                            "Group",
                                            groupId,
                                            TabGroupColorId.BLUE,
                                            /* isCollapsed= */ false)
                                    .with(
                                            TabProperties.RAIL_COLLAPSE_STATE,
                                            RailCollapseState.COLLAPSED)
                                    .build());

                    addTabListItem(
                            tabListModel,
                            createTabListItemModelBuilder("Nested Tab 1", groupId)
                                    .with(
                                            TabProperties.RAIL_COLLAPSE_STATE,
                                            RailCollapseState.COLLAPSED)
                                    .build());

                    addTabListItem(
                            tabListModel,
                            createTabListItemModelBuilder("Nested Tab 2", groupId)
                                    .with(
                                            TabProperties.RAIL_COLLAPSE_STATE,
                                            RailCollapseState.COLLAPSED)
                                    .build());

                    view[0] = container;
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(
                mRenderView, "vertical_tab_collapsed_rail" + (mIsIncognito ? "_incognito" : ""));
    }

    // =========================================================================================
    // Pinned Tabs Grid Dynamic Balancing Tests
    // =========================================================================================

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTabsGrid_SingleRow_FiveTabs() throws IOException {
        testPinnedTabsGrid(
                /* numTabs= */ 5, EXPANDED_RAIL_WIDTH_DP, "pinned_tabs_grid_single_row_five_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTabsGrid_MultiRow_SixTabs() throws IOException {
        testPinnedTabsGrid(
                /* numTabs= */ 6, EXPANDED_RAIL_WIDTH_DP, "pinned_tabs_grid_multi_row_six_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTabsGrid_MultiRow_SevenTabs() throws IOException {
        testPinnedTabsGrid(
                /* numTabs= */ 7, EXPANDED_RAIL_WIDTH_DP, "pinned_tabs_grid_multi_row_seven_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTabsGrid_NarrowRail_FiveTabs() throws IOException {
        testPinnedTabsGrid(
                /* numTabs= */ 5,
                VerticalTabUtils.MIN_EXPANDED_WIDTH_DP,
                "pinned_tabs_grid_narrow_rail_five_tabs");
    }

    // =========================================================================================
    // Tab Group Hover Card Tests
    // =========================================================================================

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHoverCard_Standard() throws IOException {
        testTabGroupHoverCard(
                "Standard Group",
                List.of("Google Search", "Wikipedia", "Chromium Issue Tracker"),
                /* excessCount= */ 0,
                "tab_group_hover_card_standard");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHoverCard_SingleTab() throws IOException {
        testTabGroupHoverCard(
                "Single Tab Group",
                List.of("YouTube - Video"),
                /* excessCount= */ 0,
                "tab_group_hover_card_single_tab");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHoverCard_MaxPreview_NoExcess() throws IOException {
        testTabGroupHoverCard(
                "Max Preview Group",
                List.of("Tab 1", "Tab 2", "Tab 3", "Tab 4", "Tab 5"),
                /* excessCount= */ 0,
                "tab_group_hover_card_max_preview");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHoverCard_LargeGroup_WithExcessTabs() throws IOException {
        testTabGroupHoverCard(
                "Large Group",
                List.of("Tab 1", "Tab 2", "Tab 3", "Tab 4", "Tab 5"),
                /* excessCount= */ 7,
                "tab_group_hover_card_excess_tabs");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHoverCard_LongTitles() throws IOException {
        testTabGroupHoverCard(
                "Very Long Tab Group Title That Truncates With Ellipsis In Hover Card",
                List.of(
                        "Very Long Tab Title 1 That Exceeds The Maximum Allowed Card Width Limit",
                        "Very Long Tab Title 2 That Exceeds The Maximum Allowed Card Width Limit"),
                /* excessCount= */ 10,
                "tab_group_hover_card_long_titles");
    }

    // =========================================================================================
    // Multi-Selection Tests
    // =========================================================================================

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_MultiSelected() throws IOException {
        testStandardTabMultiSelected(
                "Multi-Selected Tab", /* isHovered= */ false, "standard_tab_multi_selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_MultiSelected_Hovered() throws IOException {
        testStandardTabMultiSelected(
                "Multi-Selected Tab", /* isHovered= */ true, "standard_tab_multi_selected_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_MultiSelected() throws IOException {
        testPinnedTabMultiSelected(
                "Pinned Tab", /* isHovered= */ false, "pinned_tab_multi_selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_MultiSelected_Hovered() throws IOException {
        testPinnedTabMultiSelected(
                "Pinned Tab", /* isHovered= */ true, "pinned_tab_multi_selected_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testVerticalTabList_MultiSelected() throws IOException {
        TabListRecyclerView[] view = new TabListRecyclerView[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabListRecyclerView recyclerView =
                            (TabListRecyclerView)
                                    inflateAndAttachView(R.layout.tab_list_recycler_view_layout);
                    recyclerView.setVisibility(View.VISIBLE);
                    recyclerView.setLayoutManager(new LinearLayoutManager(mActivity));

                    TabListModel tabListModel = new TabListModel();
                    recyclerView.setAdapter(createTabListAdapter(tabListModel));
                    view[0] = recyclerView;

                    addTabListItem(
                            tabListModel,
                            createTabListItemModelBuilder("Active Tab", /* groupId= */ null)
                                    .with(TabProperties.IS_SELECTED, true)
                                    .build());
                    addTabListItem(
                            tabListModel,
                            createTabListItemModelBuilder("Multi-Selected Tab", /* groupId= */ null)
                                    .with(TabProperties.IS_MULTI_SELECTED, true)
                                    .build());
                    addTabListItem(
                            tabListModel,
                            createTabListItemModelBuilder("Standard Tab", /* groupId= */ null)
                                    .build());
                });

        CriteriaHelper.pollUiThread(() -> view[0].getChildCount() > 0);
        mRenderTestRule.render(
                mRenderView,
                "vertical_tab_list_multi_selected" + (mIsIncognito ? "_incognito" : ""));
    }

    // =========================================================================================
    // Tab Hover Card Tests
    // =========================================================================================

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabHoverCard_Standard() throws IOException {
        testTabHoverCard(
                "Google Search",
                JUnitTestGURLs.SEARCH_URL,
                /* isPinned= */ false,
                TabAlert.NONE,
                /* memoryUsageBytes= */ 0L,
                createThumbnailBitmap(Color.GRAY),
                "tab_hover_card_standard");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabHoverCard_Pinned() throws IOException {
        testTabHoverCard(
                "Google Search",
                JUnitTestGURLs.SEARCH_URL,
                /* isPinned= */ true,
                TabAlert.NONE,
                /* memoryUsageBytes= */ 0L,
                createThumbnailBitmap(Color.BLUE),
                "tab_hover_card_pinned");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabHoverCard_AlertAndMemoryUsage() throws IOException {
        testTabHoverCard(
                "YouTube - Video",
                JUnitTestGURLs.SEARCH_URL,
                /* isPinned= */ false,
                TabAlert.AUDIO_PLAYING,
                /* memoryUsageBytes= */ 100_000_000L,
                createThumbnailBitmap(Color.RED),
                "tab_hover_card_alert_and_memory_usage");
    }

    // =========================================================================================
    // Empty Space Context Menu (TabStripContextMenuCoordinator) Tests
    // =========================================================================================

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testEmptySpaceContextMenu_Standard() throws IOException {
        testEmptySpaceContextMenu(
                /* tabCount= */ 3,
                TabModel.RecentlyClosedEntryType.TAB,
                /* canToggleLayout= */ true,
                "tab_strip_empty_space_context_menu_standard");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testEmptySpaceContextMenu_SingleTab() throws IOException {
        // If tabCount == 1, "Bookmark all tabs" should be greyed out.
        // If incognito == true, then "Bookmark all tabs" shouldn't appear at all in general.
        testEmptySpaceContextMenu(
                /* tabCount= */ 1,
                TabModel.RecentlyClosedEntryType.TAB,
                /* canToggleLayout= */ true,
                "tab_strip_empty_space_context_menu_single_tab");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testEmptySpaceContextMenu_LayoutToggleDisabled() throws IOException {
        // If canToggleLayout == false, "Show Tabs Horizontally" should be greyed out.
        testEmptySpaceContextMenu(
                /* tabCount= */ 3,
                TabModel.RecentlyClosedEntryType.TAB,
                /* canToggleLayout= */ false,
                "tab_strip_empty_space_context_menu_layout_toggle_disabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testEmptySpaceContextMenu_NoRecentlyClosed() throws IOException {
        // If RecentlyClosedEntryType.NONE, "Reopen closed tab" should be greyed out.
        testEmptySpaceContextMenu(
                /* tabCount= */ 2,
                TabModel.RecentlyClosedEntryType.NONE,
                /* canToggleLayout= */ true,
                "tab_strip_empty_space_context_menu_no_recently_closed");
    }

    private void testEmptySpaceContextMenu(
            int tabCount,
            @TabModel.RecentlyClosedEntryType int recentlyClosedType,
            boolean canToggleLayout,
            String goldenName)
            throws IOException {
        FrameLayout[] renderContainer = new FrameLayout[1];
        // To ensure "Name window" is included in the render output.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {

                    // Force the -sw600dp resource qualifier bucket so DeviceFormFactor
                    // detects SCREEN_BUCKET_TABLET on CQ phone bots.
                    Configuration config = mActivity.getResources().getConfiguration();
                    config.smallestScreenWidthDp = 600;
                    mActivity
                            .getResources()
                            .updateConfiguration(
                                    config, mActivity.getResources().getDisplayMetrics());

                    // Configure the TabModel mock.
                    Profile mockProfile = mock(Profile.class);
                    TabModel tabModel = mock(TabModel.class);
                    when(tabModel.isIncognitoBranded()).thenReturn(mIsIncognito);
                    when(tabModel.getCount()).thenReturn(tabCount);
                    when(tabModel.getProfile()).thenReturn(mockProfile);
                    when(tabModel.getMostRecentlyClosedEntryType()).thenReturn(recentlyClosedType);

                    MultiInstanceManager multiInstanceManager = mock(MultiInstanceManager.class);
                    SnackbarManager snackbarManager = mock(SnackbarManager.class);

                    WindowAndroid windowAndroid = mock(WindowAndroid.class);
                    when(windowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
                    when(windowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));

                    TabStripContextMenuCoordinator coordinator =
                            TabStripContextMenuCoordinator.createContextMenuCoordinator(
                                    tabModel,
                                    multiInstanceManager,
                                    windowAndroid,
                                    snackbarManager,
                                    /* onNewTabClick= */ () -> {},
                                    /* canActivateTabLayoutToggleMenuSupplier= */ () ->
                                            canToggleLayout,
                                    TabContextMenuCoordinator.TabStripLayoutType.VERTICAL);

                    // Generate the complete menu list with all rows, dividers, text, click
                    // delegates.
                    View menuContentView = coordinator.buildMenuView(mIsIncognito);

                    // Since this is a render test, instead of depending on the Android Popup
                    // Window, wrap the contentView (a transparent list of menu rows) in a
                    // FrameLayout with the same background drawable to replicate the popup
                    // container.
                    renderContainer[0] = new FrameLayout(mActivity);
                    Drawable background =
                            TabOverflowMenuCoordinator.getMenuBackground(mActivity, mIsIncognito);
                    renderContainer[0].setBackground(background);

                    // User the same minWidthPx used in TabStripContextMenuCoordinator.
                    int minWidthPx =
                            mActivity
                                    .getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.tab_strip_context_menu_min_width);

                    renderContainer[0].addView(
                            menuContentView,
                            new FrameLayout.LayoutParams(
                                    minWidthPx, ViewGroup.LayoutParams.WRAP_CONTENT));

                    // Attach renderContainer to the BlankUiTestActivity window hierarchy to measure
                    // dimensions and draw pixels to the canvas.
                    mActivity.setContentView(
                            renderContainer[0],
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.WRAP_CONTENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT));
                });
        // Verify that the Android layout engine has measured the view hierarchy and assigned a
        // non-zero height.
        CriteriaHelper.pollUiThread(() -> renderContainer[0].getHeight() > 0);
        // Capture the pixel bitmap.
        mRenderTestRule.render(renderContainer[0], goldenName + (mIsIncognito ? "_incognito" : ""));
    }

    private void testTabGroupSpine(boolean isCollapsed, boolean isRtl, boolean isHeaderOffScreen)
            throws IOException {
        TabListRecyclerView[] view = new TabListRecyclerView[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabListRecyclerView recyclerView;
                    if (isHeaderOffScreen) {
                        ViewGroup container = inflateAndAttachView(R.layout.vertical_tab_layout);
                        int widthPx = ViewUtils.dpToPx(mActivity, EXPANDED_RAIL_WIDTH_DP);
                        int heightPx = ViewUtils.dpToPx(mActivity, RAIL_TEST_HEIGHT_DP);
                        container.setLayoutParams(new FrameLayout.LayoutParams(widthPx, heightPx));
                        recyclerView = container.findViewById(R.id.tab_list_recycler_view);
                    } else {
                        recyclerView =
                                (TabListRecyclerView)
                                        inflateAndAttachView(
                                                R.layout.tab_list_recycler_view_layout);
                    }

                    recyclerView.setVisibility(View.VISIBLE);
                    if (isRtl) {
                        recyclerView.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
                    }
                    recyclerView.setLayoutManager(new LinearLayoutManager(mActivity));

                    TabListModel tabListModel = new TabListModel();
                    TabModel tabModel = mock(TabModel.class);
                    when(tabModel.isIncognitoBranded()).thenReturn(mIsIncognito);
                    TabModelSelector tabModelSelector = setupMockTabModelSelector(tabModel);

                    recyclerView.addItemDecoration(
                            new VerticalTabGroupSpineDecoration(
                                    mActivity, () -> {}, tabListModel, tabModelSelector));
                    recyclerView.setAdapter(createTabListAdapter(tabListModel));
                    view[0] = recyclerView;

                    // Build mock layout.
                    Token groupId = Token.createRandom();
                    addGroupHeaderListItem(
                            tabListModel,
                            createGroupHeaderItemModelBuilder(
                                            "Group", groupId, TabGroupColorId.BLUE, isCollapsed)
                                    .build());
                    when(tabModel.getTabGroupColorWithFallback(groupId))
                            .thenReturn(TabGroupColorId.BLUE);
                    if (!isCollapsed) {
                        addTabListItem(
                                tabListModel,
                                createTabListItemModelBuilder("Test Tab 1", groupId).build());
                        addTabListItem(
                                tabListModel,
                                createTabListItemModelBuilder("Test Tab 2", groupId).build());

                        if (isHeaderOffScreen) {
                            for (int i = 1; i <= 15; i++) {
                                addTabListItem(
                                        tabListModel,
                                        createTabListItemModelBuilder("Test Tab " + i, groupId)
                                                .build());
                            }
                        }
                    }
                    addTabListItem(
                            tabListModel,
                            createTabListItemModelBuilder("Next Tab", /* groupId= */ null).build());
                });

        CriteriaHelper.pollUiThread(() -> view[0].getChildCount() > 0);
        if (isHeaderOffScreen) {
            Assert.assertNotNull(view[0].getLayoutManager());
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            ((LinearLayoutManager) view[0].getLayoutManager())
                                    .scrollToPositionWithOffset(1, 0));
            CriteriaHelper.pollUiThread(
                    () ->
                            ((LinearLayoutManager) view[0].getLayoutManager())
                                            .findFirstVisibleItemPosition()
                                    >= 1);
        }
        mRenderTestRule.render(
                mRenderView,
                "tab_group_spine"
                        + (mIsIncognito ? "_incognito" : "")
                        + (isCollapsed ? "_collapsed" : "_expanded")
                        + (isHeaderOffScreen ? "_header_off_screen" : "")
                        + (isRtl ? "_rtl" : ""));
    }

    private void testStandardTab(
            String title,
            boolean isSelected,
            boolean isLoading,
            boolean isHovered,
            String goldenName)
            throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            createTabListItemModelBuilder(
                                            (mIsIncognito ? "Incognito " : "") + title,
                                            /* groupId= */ null)
                                    .with(TabProperties.IS_SELECTED, isSelected)
                                    .with(TabProperties.IS_LOADING, isLoading)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE,
                                                    /* tabActionListener= */ null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        if (isHovered) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        MotionEvent event =
                                MotionEvent.obtain(
                                        0, 0, MotionEvent.ACTION_HOVER_ENTER, 0.0f, 0.0f, 0);
                        view[0].dispatchGenericMotionEvent(event);
                    });
        }

        String finalGoldenName =
                mIsIncognito
                        ? goldenName.replace("standard_tab_", "standard_incognito_tab_")
                        : goldenName;
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    private void testStandardTabMultiSelected(String title, boolean isHovered, String goldenName)
            throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            createTabListItemModelBuilder(
                                            (mIsIncognito ? "Incognito " : "") + title,
                                            /* groupId= */ null)
                                    .with(TabProperties.IS_SELECTED, false)
                                    .with(TabProperties.IS_MULTI_SELECTED, true)
                                    .with(
                                            TabProperties.TAB_ACTION_BUTTON_DATA,
                                            new TabActionButtonData(
                                                    TabActionButtonType.CLOSE,
                                                    /* tabActionListener= */ null))
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        if (isHovered) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        MotionEvent event =
                                MotionEvent.obtain(
                                        0, 0, MotionEvent.ACTION_HOVER_ENTER, 0.0f, 0.0f, 0);
                        view[0].dispatchGenericMotionEvent(event);
                    });
        }

        String finalGoldenName =
                mIsIncognito
                        ? goldenName.replace("standard_tab_", "standard_incognito_tab_")
                        : goldenName;
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    private void testPinnedTab(
            String title,
            boolean isSelected,
            boolean isLoading,
            boolean isHovered,
            String goldenName)
            throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] =
                            inflateAndAttachView(
                                    R.layout.vertical_tab_pinned_item, mPinnedItemWidthPx);
                    PropertyModel model =
                            createTabListItemModelBuilder(
                                            (mIsIncognito ? "Incognito " : "") + title,
                                            /* groupId= */ null)
                                    .with(TabProperties.IS_SELECTED, isSelected)
                                    .with(TabProperties.IS_PINNED, true)
                                    .with(TabProperties.IS_LOADING, isLoading)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindPinnedTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        if (isHovered) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        MotionEvent event =
                                MotionEvent.obtain(
                                        0, 0, MotionEvent.ACTION_HOVER_ENTER, 0.0f, 0.0f, 0);
                        view[0].dispatchGenericMotionEvent(event);
                    });
        }

        String finalGoldenName =
                mIsIncognito
                        ? goldenName.replace("pinned_tab_", "pinned_incognito_tab_")
                        : goldenName;
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    private void testPinnedTabMultiSelected(String title, boolean isHovered, String goldenName)
            throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] =
                            inflateAndAttachView(
                                    R.layout.vertical_tab_pinned_item, mPinnedItemWidthPx);
                    PropertyModel model =
                            createTabListItemModelBuilder(
                                            (mIsIncognito ? "Incognito " : "") + title,
                                            /* groupId= */ null)
                                    .with(TabProperties.IS_SELECTED, false)
                                    .with(TabProperties.IS_PINNED, true)
                                    .with(TabProperties.IS_MULTI_SELECTED, true)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindPinnedTab);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        if (isHovered) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        MotionEvent event =
                                MotionEvent.obtain(
                                        0, 0, MotionEvent.ACTION_HOVER_ENTER, 0.0f, 0.0f, 0);
                        view[0].dispatchGenericMotionEvent(event);
                    });
        }

        String finalGoldenName =
                mIsIncognito
                        ? goldenName.replace("pinned_tab_", "pinned_incognito_tab_")
                        : goldenName;
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    private void testTabGroupHeader(
            String title, boolean isCollapsed, boolean isHovered, String goldenName)
            throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_group_header);
                    PropertyModel model =
                            createGroupHeaderItemModelBuilder(
                                            (mIsIncognito ? "Incognito " : "") + title,
                                            /* headerId= */ null,
                                            TabGroupColorId.BLUE,
                                            isCollapsed)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTabGroupHeader);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        if (isHovered) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        MotionEvent event =
                                MotionEvent.obtain(
                                        0, 0, MotionEvent.ACTION_HOVER_ENTER, 0.0f, 0.0f, 0);
                        view[0].dispatchGenericMotionEvent(event);
                    });
        }

        String finalGoldenName =
                mIsIncognito
                        ? goldenName.replace("tab_group_header_", "tab_group_header_incognito_")
                        : goldenName;
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    private void testTabGroupHoverCard(
            String title, List<String> childTabTitles, int excessCount, String goldenName)
            throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.tab_group_hover_card_holder);
                    TabGroupHoverCardView hoverCardView = (TabGroupHoverCardView) view[0];
                    hoverCardView.bindData(
                            (mIsIncognito ? "Incognito " : "") + title,
                            childTabTitles,
                            excessCount,
                            mIsIncognito);
                    hoverCardView.show(/* x= */ 0, /* y= */ 0);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        String finalGoldenName =
                mIsIncognito
                        ? goldenName.replace(
                                "tab_group_hover_card_", "tab_group_hover_card_incognito_")
                        : goldenName;
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    private void testPinnedTabsGrid(int numTabs, int railWidthDp, String goldenName)
            throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int widthPx = ViewUtils.dpToPx(mActivity, railWidthDp);
                    TabListRecyclerView pinnedRecyclerView =
                            (TabListRecyclerView)
                                    inflateAndAttachView(
                                            R.layout.tab_list_recycler_view_layout, widthPx);
                    pinnedRecyclerView.setVisibility(View.VISIBLE);
                    int availableWidth =
                            widthPx
                                    - pinnedRecyclerView.getPaddingStart()
                                    - pinnedRecyclerView.getPaddingEnd();
                    int spanCount =
                            VerticalTabListCoordinator.calculateBalancedSpanCount(
                                    availableWidth,
                                    numTabs,
                                    mActivity.getResources(),
                                    VerticalTabUtils.isTablet(mActivity));

                    pinnedRecyclerView.setLayoutManager(
                            new GridLayoutManager(mActivity, spanCount));
                    pinnedRecyclerView.addItemDecoration(
                            VerticalTabListCoordinator.createPinnedTabItemDecoration());

                    TabListModel pinnedTabsModel = new TabListModel();
                    pinnedRecyclerView.setAdapter(createPinnedTabListAdapter(pinnedTabsModel));

                    for (int i = 0; i < numTabs; i++) {
                        PropertyModel pinnedTabModel =
                                createTabListItemModelBuilder(
                                                "Pinned Tab " + (i + 1), /* groupId= */ null)
                                        .with(TabProperties.IS_PINNED, true)
                                        .with(
                                                TabProperties.RAIL_COLLAPSE_STATE,
                                                RailCollapseState.EXPANDED)
                                        .with(TabProperties.IS_SELECTED, i == 0)
                                        .build();
                        addPinnedTabListItem(pinnedTabsModel, pinnedTabModel);
                    }

                    view[0] = pinnedRecyclerView;
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        String finalGoldenName =
                mIsIncognito
                        ? goldenName.replace("pinned_tabs_grid_", "pinned_tabs_grid_incognito_")
                        : goldenName;
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    private Bitmap createThumbnailBitmap(@ColorInt int color) {
        Bitmap bitmap = Bitmap.createBitmap(300, 200, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(color);
        return bitmap;
    }

    private void testTabHoverCard(
            String title,
            GURL url,
            boolean isPinned,
            @TabAlert int alertState,
            long memoryUsageBytes,
            @Nullable Bitmap thumbnail,
            String goldenName)
            throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.tab_hover_card_holder);
                    TabHoverCardView hoverCardView = (TabHoverCardView) view[0];

                    TabModel tabModel = mock(TabModel.class);
                    when(tabModel.isIncognitoBranded()).thenReturn(mIsIncognito);
                    TabModelSelector tabModelSelector = setupMockTabModelSelector(tabModel);

                    TabContentManager tabContentManager = mock(TabContentManager.class);
                    if (thumbnail != null) {
                        doAnswer(
                                        invocation -> {
                                            Callback<Bitmap> callback = invocation.getArgument(2);
                                            callback.onResult(thumbnail);
                                            return null;
                                        })
                                .when(tabContentManager)
                                .getTabThumbnailWithCallback(anyInt(), any(), any());
                    }
                    hoverCardView.initialize(tabModelSelector, () -> tabContentManager);

                    Tab tab = createMockTab(1, title, url, isPinned, alertState, memoryUsageBytes);
                    hoverCardView.show(/* x= */ 0, /* y= */ 0);
                    hoverCardView.bindTab(tab);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean isHeightValid = view[0].getHeight() > 0;
                    boolean isMemoryValid =
                            memoryUsageBytes == 0
                                    || view[0].findViewById(R.id.memory_usage).getVisibility()
                                            == View.VISIBLE;
                    TabThumbnailView thumbnailView = view[0].findViewById(R.id.thumbnail);
                    boolean isThumbnailValid =
                            thumbnail == null || thumbnailView.getDrawable() != null;
                    return isHeightValid && isMemoryValid && isThumbnailValid;
                });

        String finalGoldenName =
                mIsIncognito
                        ? goldenName.replace("tab_hover_card_", "tab_hover_card_incognito_")
                        : goldenName;
        mRenderTestRule.render(mRenderView, finalGoldenName);
    }

    private Tab createMockTab(
            int id,
            String title,
            GURL url,
            boolean isPinned,
            @TabAlert int alertState,
            long memoryUsageBytes) {
        Tab tab = mock(Tab.class);
        when(tab.getId()).thenReturn(id);
        when(tab.getTitle()).thenReturn((mIsIncognito ? "Incognito " : "") + title);
        when(tab.getUrl()).thenReturn(url);
        when(tab.getIsPinned()).thenReturn(isPinned);
        when(tab.isIncognito()).thenReturn(mIsIncognito);
        when(tab.getAlertState()).thenReturn(alertState);
        doAnswer(
                        invocation -> {
                            Callback<Long> callback = invocation.getArgument(0);
                            callback.onResult(memoryUsageBytes);
                            return null;
                        })
                .when(tab)
                .getMemoryUsageBytes(any());
        return tab;
    }

    private TabModelSelector setupMockTabModelSelector(TabModel tabModel) {
        TabModelSelector tabModelSelector = mock(TabModelSelector.class);
        when(tabModelSelector.getCurrentModel()).thenReturn(tabModel);

        var supplier = ObservableSuppliers.<TabModel>createMonotonic();
        supplier.set(tabModel);
        when(tabModelSelector.getCurrentTabModelSupplier()).thenReturn(supplier);
        return tabModelSelector;
    }

    private SimpleRecyclerViewAdapter createTabListAdapter(TabListModel tabListModel) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(tabListModel);
        adapter.registerType(
                UiType.TAB_GROUP,
                parent -> inflateView(R.layout.vertical_tab_group_header, parent),
                TabVerticalViewBinder::bindTabGroupHeader);
        adapter.registerType(
                UiType.TAB,
                parent -> inflateView(R.layout.vertical_tab_item, parent),
                TabVerticalViewBinder::bindTab);
        return adapter;
    }

    private SimpleRecyclerViewAdapter createPinnedTabListAdapter(TabListModel pinnedTabsModel) {
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(pinnedTabsModel);
        adapter.registerType(
                UiType.PINNED_TAB,
                parent -> inflateView(R.layout.vertical_tab_pinned_item, parent),
                TabVerticalViewBinder::bindPinnedTab);
        return adapter;
    }

    private PropertyModel.Builder createTabListItemModelBuilder(
            String title, @Nullable Token groupId) {
        return new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                .with(TabProperties.TITLE, title)
                .with(TabProperties.IS_SELECTED, false)
                .with(TabProperties.TAB_GROUP_ID, groupId)
                .with(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
    }

    private PropertyModel.Builder createGroupHeaderItemModelBuilder(
            String title,
            Token headerId,
            @Nullable @TabGroupColorId Integer color,
            boolean isCollapsed) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(TabProperties.TITLE, title)
                        .with(TabProperties.IS_SELECTED, false)
                        .with(TabProperties.TAB_GROUP_HEADER_ID, headerId)
                        .with(TabProperties.IS_COLLAPSED, isCollapsed);

        if (color != null) {
            builder.with(TabProperties.TAB_GROUP_CARD_COLOR, color);
        }
        return builder;
    }

    private void addTabListItem(TabListModel tabListModel, PropertyModel model) {
        tabListModel.add(new MVCListAdapter.ListItem(UiType.TAB, model));
    }

    private void addPinnedTabListItem(TabListModel tabListModel, PropertyModel model) {
        tabListModel.add(new MVCListAdapter.ListItem(UiType.PINNED_TAB, model));
    }

    private void addGroupHeaderListItem(TabListModel tabListModel, PropertyModel model) {
        tabListModel.add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, model));
    }
}
