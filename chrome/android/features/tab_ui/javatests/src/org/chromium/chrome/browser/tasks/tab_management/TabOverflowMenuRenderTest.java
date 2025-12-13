// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;
import java.util.List;
import java.util.function.Supplier;

/** Render tests for {@link TabOverflowMenuCoordinator}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class TabOverflowMenuRenderTest {
    private static final int MENU_WIDTH = 1200;

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_CONTEXT_MENU)
                    .setRevision(1)
                    .build();

    @Mock private TabOverflowMenuCoordinator.OnItemClickedCallback<Integer> mOnItemClickedCallback;
    @Mock private Supplier<TabModel> mTabModelSupplier;
    @Mock private TabModel mTabModel;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private CollaborationService mCollaborationService;
    @Mock private ServiceStatus mServiceStatus;

    private Activity mActivity;
    private View mView;
    private TabOverflowMenuCoordinator<Integer> mCoordinator;

    public TabOverflowMenuRenderTest(boolean isNightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(isNightModeEnabled);
        mRenderTestRule.setNightModeEnabled(isNightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.OverflowMenuThemeOverlay);

        when(mTabModelSupplier.get()).thenReturn(mTabModel);
        when(mCollaborationService.getServiceStatus()).thenReturn(mServiceStatus);
        CollaborationServiceFactory.setForTesting(mCollaborationService);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator.destroyMenuForTesting();
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderMenu() throws IOException {
        showMenu();
        View rootView = mView.getRootView();
        mRenderTestRule.render(rootView, "tab_overflow_menu");
    }

    private void showMenu() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new TabOverflowMenuCoordinator<>(
                                    R.layout.tab_switcher_action_menu_layout,
                                    R.layout.tab_switcher_action_menu_layout,
                                    mOnItemClickedCallback,
                                    mTabModelSupplier,
                                    mMultiInstanceManager,
                                    mTabGroupSyncService,
                                    mCollaborationService,
                                    mActivity) {
                                @Override
                                protected void buildMenuActionItems(
                                        ModelList itemList, Integer id) {
                                    itemList.add(
                                            new ListItemBuilder()
                                                    .withTitle("TEST_TITLE1")
                                                    .withStartIconRes(
                                                            R.drawable.tab_list_editor_share_icon)
                                                    .build());
                                    itemList.add(
                                            new ListItemBuilder()
                                                    .withTitle("TEST_TITLE2")
                                                    .withStartIconRes(R.drawable.ic_widgets)
                                                    .build());
                                    itemList.add(
                                            new ListItemBuilder()
                                                    .withTitle("TEST_TITLE3")
                                                    .withStartIconRes(R.drawable.btn_star_filled)
                                                    .build());
                                }

                                @Override
                                protected int getMenuWidth(int anchorViewWidthPx) {
                                    return MENU_WIDTH;
                                }

                                @Override
                                protected String getCollaborationIdOrNull(Integer id) {
                                    return null;
                                }
                            };

                    mCoordinator.createAndShowMenu(new View(mActivity), 1, mActivity);
                    mView = mCoordinator.getContentViewForTesting();
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollUiThread(
                () -> mView.getWidth() > 0 && mView.getHeight() > 0, "View not rendered");
    }
}
