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
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.test.filters.MediumTest;

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
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(1)
                    .build();

    private final boolean mIsNightModeEnabled;
    private Activity mActivity;
    private FrameLayout mRenderView;

    public VerticalTabListRenderTest(boolean isNightModeEnabled) {
        mIsNightModeEnabled = isNightModeEnabled;
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

    private TabFaviconFetcher createFaviconFetcher(boolean isIncognito) {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> callback) {
                Drawable drawable =
                        AppCompatResources.getDrawable(
                                mActivity,
                                isIncognito
                                        ? R.drawable.ic_incognito_24dp
                                        : R.drawable.ic_globe_24dp);
                callback.onResult(
                        new TabFavicon(drawable, drawable, false) {
                            @Override
                            public boolean equals(Object other) {
                                return false;
                            }
                        });
            }
        };
    }

    private TabFaviconFetcher createFaviconFetcher() {
        return createFaviconFetcher(/* isIncognito= */ false);
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
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
                "standard_tab_loading");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_ActorIndicator_Dynamic() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "AI Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false);
                    model.set(TabProperties.ACTOR_UI_STATE, uiTabState);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_actor_indicator_dynamic");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_ActorIndicator_Static() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "AI Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.STATIC, false);
                    model.set(TabProperties.ACTOR_UI_STATE, uiTabState);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_actor_indicator_static");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Active_ActorIndicator_Dynamic() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Active AI Tab");
                    model.set(TabProperties.IS_SELECTED, true);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false);
                    model.set(TabProperties.ACTOR_UI_STATE, uiTabState);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_active_actor_indicator_dynamic");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Active_ActorIndicator_Static() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Active AI Tab");
                    model.set(TabProperties.IS_SELECTED, true);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.STATIC, false);
                    model.set(TabProperties.ACTOR_UI_STATE, uiTabState);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_active_actor_indicator_static");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_MediaIndicator() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Media Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(TabProperties.MEDIA_INDICATOR, MediaState.AUDIBLE);
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
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
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
                "pinned_tab_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Collapsed() throws IOException {
        testTabGroupHeader(
                "Collapsed Group",
                /* isCollapsed= */ true,
                /* isHovered= */ false,
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
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
                /* isIncognito= */ false,
                "tab_group_header_expanded_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardIncognitoTab_Unselected() throws IOException {
        testStandardTab(
                "Standard Tab",
                /* isSelected= */ false,
                /* isLoading= */ false,
                /* isHovered= */ false,
                /* isIncognito= */ true,
                "standard_incognito_tab_unselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardIncognitoTab_Active() throws IOException {
        testStandardTab(
                "Active Tab",
                /* isSelected= */ true,
                /* isLoading= */ false,
                /* isHovered= */ false,
                /* isIncognito= */ true,
                "standard_incognito_tab_active");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardIncognitoTab_Loading() throws IOException {
        testStandardTab(
                "Loading Tab",
                /* isSelected= */ false,
                /* isLoading= */ true,
                /* isHovered= */ false,
                /* isIncognito= */ true,
                "standard_incognito_tab_loading");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardIncognitoTab_Hovered() throws IOException {
        testStandardTab(
                "Hovered Tab",
                /* isSelected= */ false,
                /* isLoading= */ false,
                /* isHovered= */ true,
                /* isIncognito= */ true,
                "standard_incognito_tab_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedIncognitoTab_Unselected() throws IOException {
        testPinnedTab(
                "Pinned Tab",
                /* isSelected= */ false,
                /* isLoading= */ false,
                /* isHovered= */ false,
                /* isIncognito= */ true,
                "pinned_incognito_tab_unselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedIncognitoTab_Active() throws IOException {
        testPinnedTab(
                "Pinned Tab",
                /* isSelected= */ true,
                /* isLoading= */ false,
                /* isHovered= */ false,
                /* isIncognito= */ true,
                "pinned_incognito_tab_active");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedIncognitoTab_Loading() throws IOException {
        testPinnedTab(
                "Loading Pinned Tab",
                /* isSelected= */ false,
                /* isLoading= */ true,
                /* isHovered= */ false,
                /* isIncognito= */ true,
                "pinned_incognito_tab_loading");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedIncognitoTab_Hovered() throws IOException {
        testPinnedTab(
                "Hovered Pinned Tab",
                /* isSelected= */ false,
                /* isLoading= */ false,
                /* isHovered= */ true,
                /* isIncognito= */ true,
                "pinned_incognito_tab_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Incognito_Collapsed() throws IOException {
        testTabGroupHeader(
                "Collapsed Group",
                /* isCollapsed= */ true,
                /* isHovered= */ false,
                /* isIncognito= */ true,
                "tab_group_header_incognito_collapsed");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Incognito_Expanded() throws IOException {
        testTabGroupHeader(
                "Expanded Group",
                /* isCollapsed= */ false,
                /* isHovered= */ false,
                /* isIncognito= */ true,
                "tab_group_header_incognito_expanded");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Incognito_Collapsed_Hovered() throws IOException {
        testTabGroupHeader(
                "Hovered Group (Collapsed)",
                /* isCollapsed= */ true,
                /* isHovered= */ true,
                /* isIncognito= */ true,
                "tab_group_header_incognito_collapsed_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Incognito_Expanded_Hovered() throws IOException {
        testTabGroupHeader(
                "Hovered Group (Expanded)",
                /* isCollapsed= */ false,
                /* isHovered= */ true,
                /* isIncognito= */ true,
                "tab_group_header_incognito_expanded_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Expanded() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ false,
                /* isRtl= */ false,
                /* isHeaderOffScreen= */ false,
                /* isIncognito= */ false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Expanded_Rtl() throws IOException {
        LocalizationUtils.setRtlForTesting(true);
        try {
            testTabGroupSpine(
                    /* isCollapsed= */ false,
                    /* isRtl= */ true,
                    /* isHeaderOffScreen= */ false,
                    /* isIncognito= */ false);
        } finally {
            LocalizationUtils.setRtlForTesting(false);
        }
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Collapsed() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ true,
                /* isRtl= */ false,
                /* isHeaderOffScreen= */ false,
                /* isIncognito= */ false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_HeaderOffScreen() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ false,
                /* isRtl= */ false,
                /* isHeaderOffScreen= */ true,
                /* isIncognito= */ false);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Incognito_Expanded() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ false,
                /* isRtl= */ false,
                /* isHeaderOffScreen= */ false,
                /* isIncognito= */ true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Incognito_Collapsed() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ true,
                /* isRtl= */ false,
                /* isHeaderOffScreen= */ false,
                /* isIncognito= */ true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupSpine_Incognito_HeaderOffScreen() throws IOException {
        testTabGroupSpine(
                /* isCollapsed= */ false,
                /* isRtl= */ false,
                /* isHeaderOffScreen= */ true,
                /* isIncognito= */ true);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testHeaderContainer_Expanded() throws IOException {
        testHeaderContainer(/* isCollapsed= */ false, "vertical_tab_header_expanded");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testHeaderContainer_Collapsed() throws IOException {
        testHeaderContainer(/* isCollapsed= */ true, "vertical_tab_header_collapsed");
    }

    private void testHeaderContainer(boolean isCollapsed, String goldenName) throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup container = inflateAndAttachView(R.layout.vertical_tab_layout);
                    View collapseButton = container.findViewById(R.id.collapse_button);
                    if (collapseButton != null) {
                        collapseButton.setVisibility(View.VISIBLE);
                    }
                    View headerSpacer = container.findViewById(R.id.header_spacer);
                    if (headerSpacer != null) {
                        headerSpacer.setVisibility(View.VISIBLE);
                    }

                    int widthDp = isCollapsed ? 74 : 206;
                    int widthPx = ViewUtils.dpToPx(mActivity, widthDp);
                    container.setLayoutParams(
                            new FrameLayout.LayoutParams(
                                    widthPx, ViewGroup.LayoutParams.WRAP_CONTENT));

                    PropertyModel model =
                            new PropertyModel.Builder(VerticalTabListProperties.ALL_KEYS)
                                    .with(VerticalTabListProperties.IS_COLLAPSED, isCollapsed)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, container, VerticalTabListViewBinder::bind);

                    view[0] = container.findViewById(R.id.vertical_tab_header_container);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(view[0], goldenName);
    }

    private void testTabGroupSpine(
            boolean isCollapsed, boolean isRtl, boolean isHeaderOffScreen, boolean isIncognito)
            throws IOException {
        if (isIncognito && !mIsNightModeEnabled) return;

        if (isIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
        TabListRecyclerView[] view = new TabListRecyclerView[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabListRecyclerView recyclerView;
                    if (isHeaderOffScreen) {
                        ViewGroup container = inflateAndAttachView(R.layout.vertical_tab_layout);
                        int widthPx = ViewUtils.dpToPx(mActivity, 300);
                        int heightPx = ViewUtils.dpToPx(mActivity, 400);
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

                    // Mock tabModelSelector for VerticalTabGroupSpineDecoration.
                    TabModelSelector tabModelSelector = mock(TabModelSelector.class);
                    TabModel tabModel = mock(TabModel.class);
                    when(tabModelSelector.getCurrentModel()).thenReturn(tabModel);

                    var supplier = ObservableSuppliers.<TabModel>createMonotonic();
                    supplier.set(tabModel);
                    when(tabModelSelector.getCurrentTabModelSupplier()).thenReturn(supplier);

                    recyclerView.addItemDecoration(
                            new VerticalTabGroupSpineDecoration(
                                    mActivity, () -> {}, tabListModel, tabModelSelector));

                    // Set up the adapter and tie it to the views.
                    SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(tabListModel);
                    adapter.registerType(
                            UiType.TAB_GROUP,
                            parent -> inflateView(R.layout.vertical_tab_group_header, parent),
                            TabVerticalViewBinder::bindTabGroupHeader);
                    adapter.registerType(
                            UiType.TAB,
                            parent -> inflateView(R.layout.vertical_tab_item, parent),
                            TabVerticalViewBinder::bindTab);

                    recyclerView.setAdapter(adapter);
                    view[0] = recyclerView;

                    // Build mock layout.
                    Token groupId = Token.createRandom();
                    addGroupHeaderListItem(
                            tabListModel, "Group", groupId, TabGroupColorId.BLUE, isCollapsed);
                    when(tabModel.getTabGroupColorWithFallback(groupId))
                            .thenReturn(TabGroupColorId.BLUE);
                    if (!isCollapsed) {
                        addTabListItem(tabListModel, "Test Tab 1", groupId, isIncognito);
                        addTabListItem(tabListModel, "Test Tab 2", groupId, isIncognito);

                        if (isHeaderOffScreen) {
                            for (int i = 1; i <= 15; i++) {
                                addTabListItem(tabListModel, "Test Tab " + i, groupId, isIncognito);
                            }
                        }
                    }
                    addTabListItem(tabListModel, "Next Tab", /* groupId= */ null, isIncognito);
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
                        + (isIncognito ? "_incognito" : "")
                        + (isCollapsed ? "_collapsed" : "_expanded")
                        + (isHeaderOffScreen ? "_header_off_screen" : "")
                        + (isRtl ? "_rtl" : ""));
    }

    private void addTabListItem(
            TabListModel tabListModel, String title, @Nullable Token groupId, boolean isIncognito) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                        .with(TabProperties.TITLE, title)
                        .with(TabProperties.IS_SELECTED, false)
                        .with(TabProperties.TAB_GROUP_ID, groupId)
                        .with(TabProperties.FAVICON_FETCHER, createFaviconFetcher(isIncognito));

        tabListModel.add(new MVCListAdapter.ListItem(UiType.TAB, builder.build()));
    }

    private void addGroupHeaderListItem(
            TabListModel tabListModel,
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

        tabListModel.add(new MVCListAdapter.ListItem(UiType.TAB_GROUP, builder.build()));
    }

    private void testStandardTab(
            String title,
            boolean isSelected,
            boolean isLoading,
            boolean isHovered,
            boolean isIncognito,
            String goldenName)
            throws IOException {
        if (isIncognito && !mIsNightModeEnabled) return;

        if (isIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, (isIncognito ? "Incognito " : "") + title);
                    model.set(TabProperties.IS_SELECTED, isSelected);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher(isIncognito));
                    if (isLoading) {
                        model.set(TabProperties.IS_LOADING, true);
                    }
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
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

        mRenderTestRule.render(mRenderView, goldenName);
    }

    private void testPinnedTab(
            String title,
            boolean isSelected,
            boolean isLoading,
            boolean isHovered,
            boolean isIncognito,
            String goldenName)
            throws IOException {
        if (isIncognito && !mIsNightModeEnabled) return;

        if (isIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_pinned_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindPinnedTab);
                    model.set(TabProperties.TITLE, (isIncognito ? "Incognito " : "") + title);
                    model.set(TabProperties.IS_SELECTED, isSelected);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher(isIncognito));
                    if (isLoading) {
                        model.set(TabProperties.IS_LOADING, true);
                    }
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

        mRenderTestRule.render(mRenderView, goldenName);
    }

    private void testTabGroupHeader(
            String title,
            boolean isCollapsed,
            boolean isHovered,
            boolean isIncognito,
            String goldenName)
            throws IOException {
        if (isIncognito && !mIsNightModeEnabled) return;

        if (isIncognito) {
            mActivity.setTheme(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_group_header);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB).build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTabGroupHeader);
                    model.set(TabProperties.TITLE, (isIncognito ? "Incognito " : "") + title);
                    model.set(TabProperties.TAB_GROUP_CARD_COLOR, TabGroupColorId.BLUE);
                    model.set(TabProperties.IS_COLLAPSED, isCollapsed);
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

        mRenderTestRule.render(mRenderView, goldenName);
    }
}
