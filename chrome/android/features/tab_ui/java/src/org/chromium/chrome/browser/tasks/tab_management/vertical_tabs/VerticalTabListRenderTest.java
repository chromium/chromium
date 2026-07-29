// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListRecyclerView;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

// TODO(crbug.com/521987032): Add tests for nested children with actor indicator.
// TODO(crbug.com/509226293): Add tests for RTL layout.

/** Render tests for Vertical Tabs UI (TabVerticalViewBinder). */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
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
                    .setRevision(1)
                    .build();

    private final boolean mIsIncognito;
    private Activity mActivity;
    private FrameLayout mRenderView;

    public VerticalTabListRenderTest(boolean isNightModeEnabled, boolean isIncognito) {
        mIsIncognito = isIncognito;
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(isNightModeEnabled);
        mRenderTestRule.setNightModeEnabled(isNightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    private ViewGroup inflateAndAttachView(int layoutResId) {
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
        int width = ViewGroup.LayoutParams.WRAP_CONTENT;

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
        if (mIsIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_pinned_item);
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
        mRenderTestRule.render(view[0], finalGoldenName);
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
                    SimpleRecyclerViewAdapter pinnedAdapter =
                            new SimpleRecyclerViewAdapter(pinnedTabsModel);
                    pinnedAdapter.registerType(
                            UiType.PINNED_TAB,
                            parent -> inflateView(R.layout.vertical_tab_pinned_item, parent),
                            TabVerticalViewBinder::bindPinnedTab);
                    pinnedRecyclerView.setAdapter(pinnedAdapter);

                    PropertyModel pinnedTabModel =
                            createTabListItemModelBuilder("Pinned Tab", /* groupId= */ null)
                                    .with(TabProperties.IS_PINNED, true)
                                    .with(
                                            TabProperties.RAIL_COLLAPSE_STATE,
                                            RailCollapseState.COLLAPSED)
                                    .build();
                    pinnedTabsModel.add(
                            new MVCListAdapter.ListItem(UiType.PINNED_TAB, pinnedTabModel));

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

    private void testTabGroupSpine(boolean isCollapsed, boolean isRtl, boolean isHeaderOffScreen)
            throws IOException {
        if (mIsIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
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
        if (mIsIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
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

    private void testPinnedTab(
            String title,
            boolean isSelected,
            boolean isLoading,
            boolean isHovered,
            String goldenName)
            throws IOException {
        if (mIsIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_pinned_item);
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

    private void testTabGroupHeader(
            String title, boolean isCollapsed, boolean isHovered, String goldenName)
            throws IOException {
        if (mIsIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
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

    private void addGroupHeaderListItem(TabListModel tabListModel, PropertyModel model) {
        tabListModel.add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, model));
    }
}
