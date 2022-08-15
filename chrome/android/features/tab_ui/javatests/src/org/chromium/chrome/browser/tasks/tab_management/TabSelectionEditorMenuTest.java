// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.Toolbar;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * On-device Unit tests for the {@link TabSelectionEditorMenu} and its related
 * classes.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TabSelectionEditorMenuTest extends BlankUiTestActivityTestCase {
    private static final int TAB_COUNT = 3;
    private static final Integer TAB_ID_0 = 0;
    private static final Integer TAB_ID_1 = 1;
    private static final Integer TAB_ID_2 = 2;
    private static final Integer[] TAB_IDS = new Integer[] {TAB_ID_0, TAB_ID_1, TAB_ID_2};

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(0)
                    .build();

    static class FakeTabSelectionEditorAction extends TabSelectionEditorAction {
        private boolean mShouldEnableAction = true;
        private List<Integer> mLastTabIdList;

        FakeTabSelectionEditorAction(Context context, int menuId, @ShowMode int showMode,
                @ButtonType int buttonType, @IconPosition int iconPosition, int title,
                Integer iconResourceId) {
            super(menuId, showMode, buttonType, iconPosition, title,
                    R.plurals.accessibility_tab_suggestion_close_tab_action_button,
                    (iconResourceId != null) ? UiUtils.getTintedDrawable(
                            context, iconResourceId, R.color.default_icon_color_tint_list)
                                             : null);
        }

        public interface FakeActionHandler {
            public boolean onAction(List<Tab> tabs);
        }

        public void setShouldEnableAction(boolean shouldEnableAction) {
            mShouldEnableAction = shouldEnableAction;
        }

        public List<Integer> getLastTabIdList() {
            return mLastTabIdList;
        }

        @Override
        public void onSelectionStateChange(List<Integer> tabs) {
            mLastTabIdList = tabs;
            setEnabledAndItemCount(mShouldEnableAction, tabs.size());
        }

        @Override
        public void performAction(List<Tab> tabs) {}

        @Override
        public boolean shouldHideEditorAfterAction() {
            return false;
        }
    }

    @Mock
    private TabModel mTabModel;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock
    private ActionDelegate mDelegate;

    private List<Tab> mTabs = new ArrayList<>();

    private Toolbar mToolbar;
    private TabSelectionEditorMenu mTabSelectionEditorMenu;
    private PropertyListModel<PropertyModel, PropertyKey> mPropertyListModel;
    private ListModelChangeProcessor mChangeProcessor;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(TAB_COUNT);

        for (int id = 0; id < TAB_COUNT; id++) {
            Tab tab = mock(Tab.class);
            mTabs.add(tab);
            when(tab.getId()).thenReturn(TAB_IDS[id]);
            when(mTabModel.getTabAt(id)).thenReturn(tab);
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LinearLayout layout = new LinearLayout(getActivity());
            LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT);
            layout.setLayoutParams(layoutParams);
            getActivity().setContentView(layout);

            mToolbar = new Toolbar(getActivity());
            layoutParams = new LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
            mToolbar.setLayoutParams(layoutParams);
            mToolbar.setTitle("Test!");
            mToolbar.setVisibility(View.VISIBLE);
            layout.addView(mToolbar, 0);
            getActivity().setSupportActionBar(mToolbar);

            mPropertyListModel = new PropertyListModel<>();
            mTabSelectionEditorMenu = new TabSelectionEditorMenu(getActivity(), mToolbar.getMenu());
            mToolbar.setOnMenuItemClickListener(mTabSelectionEditorMenu::onMenuItemClick);
            mChangeProcessor = new ListModelChangeProcessor(mPropertyListModel,
                    mTabSelectionEditorMenu, new TabSelectionEditorMenuAdapter());
            mPropertyListModel.addObserver(mChangeProcessor);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mPropertyListModel.clear(); });
    }

    private void configureMenuWithActions(List<FakeTabSelectionEditorAction> actions) {
        List<PropertyModel> models = new ArrayList<>();
        for (FakeTabSelectionEditorAction action : actions) {
            action.getPropertyModel().set(TabSelectionEditorActionProperties.TEXT_TINT,
                    AppCompatResources.getColorStateList(
                            getActivity(), R.color.default_text_color_list));
            action.getPropertyModel().set(TabSelectionEditorActionProperties.ICON_TINT,
                    AppCompatResources.getColorStateList(
                            getActivity(), R.color.default_icon_color_tint_list));
            action.configure(mTabModelSelector, mSelectionDelegate, mDelegate);
            models.add(action.getPropertyModel());
        }
        mPropertyListModel.addAll(models, 0);
    }

    private void changeSelectionStateAndAssert(
            List<Integer> tabIds, List<FakeTabSelectionEditorAction> actions) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTabSelectionEditorMenu.onSelectionStateChange(tabIds); });
        for (FakeTabSelectionEditorAction action : actions) {
            Assert.assertEquals(tabIds.size(), action.getLastTabIdList().size());
            for (int i = 0; i < tabIds.size(); i++) {
                Assert.assertEquals(tabIds.get(i), action.getLastTabIdList().get(i));
            }
        }
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleActionView_Enabled() throws Exception {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.IF_ROOM,
                    ButtonType.ICON_AND_TEXT, IconPosition.END,
                    R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });

        changeSelectionStateAndAssert(Arrays.asList(new Integer[] {TAB_ID_0, TAB_ID_2}), actions);

        mRenderTestRule.render(mToolbar, "singleActionToolbarEnabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleActionView_Disabled() throws Exception {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.IF_ROOM,
                    ButtonType.ICON_AND_TEXT, IconPosition.END,
                    R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { actions.get(0).setShouldEnableAction(false); });
        changeSelectionStateAndAssert(new ArrayList<Integer>(), actions);

        mRenderTestRule.render(mToolbar, "singleActionToolbarDisabled");
    }

    @Test
    @MediumTest
    public void testSingleActionView_Click() throws Exception {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.IF_ROOM,
                    ButtonType.ICON_AND_TEXT, IconPosition.END,
                    R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });

        changeSelectionStateAndAssert(Arrays.asList(new Integer[] {TAB_ID_0, TAB_ID_2}), actions);

        final List<Tab> processedTabs = new ArrayList<>();
        final CallbackHelper helper = new CallbackHelper();
        final ActionObserver observer = new ActionObserver() {
            @Override
            public void preProcessSelectedTabs(List<Tab> tabs) {
                processedTabs.clear();
                processedTabs.addAll(tabs);
                helper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { actions.get(0).addActionObserver(observer); });
        when(mSelectionDelegate.getSelectedItems())
                .thenReturn(new HashSet<Integer>(Arrays.asList(new Integer[] {TAB_ID_1})));
        clickActionView(R.id.tab_selection_editor_close_menu_item);

        helper.waitForCallback(0);
        Assert.assertEquals(1, processedTabs.size());
        Assert.assertEquals(mTabs.get(1), processedTabs.get(0));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleMenuItem_Enabled() throws Exception {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.MENU_ONLY, ButtonType.TEXT,
                    IconPosition.START, R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });

        changeSelectionStateAndAssert(Arrays.asList(new Integer[] {TAB_ID_0}), actions);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.showOverflowMenu();
            Assert.assertTrue(mToolbar.getMenu()
                                      .findItem(R.id.tab_selection_editor_close_menu_item)
                                      .isVisible());
            Assert.assertTrue(mToolbar.getMenu()
                                      .findItem(R.id.tab_selection_editor_close_menu_item)
                                      .isEnabled());
        });

        mRenderTestRule.render(mToolbar, "singleMenuItemToolbar");
    }

    @Test
    @MediumTest
    public void testSingleMenuItem_Disabled() {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.MENU_ONLY, ButtonType.TEXT,
                    IconPosition.START, R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { actions.get(0).setShouldEnableAction(false); });
        changeSelectionStateAndAssert(Arrays.asList(new Integer[] {}), actions);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.showOverflowMenu();
            Assert.assertTrue(mToolbar.getMenu()
                                      .findItem(R.id.tab_selection_editor_close_menu_item)
                                      .isVisible());
            Assert.assertFalse(mToolbar.getMenu()
                                       .findItem(R.id.tab_selection_editor_close_menu_item)
                                       .isEnabled());
        });
    }

    @Test
    @MediumTest
    public void testSingleMenuItem_Click() throws Exception {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.MENU_ONLY, ButtonType.TEXT,
                    IconPosition.START, R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });
        changeSelectionStateAndAssert(Arrays.asList(new Integer[] {TAB_ID_0, TAB_ID_2}), actions);

        final List<Tab> processedTabs = new ArrayList<>();
        final CallbackHelper helper = new CallbackHelper();
        final ActionObserver observer = new ActionObserver() {
            @Override
            public void preProcessSelectedTabs(List<Tab> tabs) {
                processedTabs.clear();
                processedTabs.addAll(tabs);
                helper.notifyCalled();
            }
        };
        when(mSelectionDelegate.getSelectedItems())
                .thenReturn(new HashSet<Integer>(Arrays.asList(new Integer[] {TAB_ID_2})));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.showOverflowMenu();
            actions.get(0).addActionObserver(observer);
        });
        clickMenuItem(R.id.tab_selection_editor_close_menu_item, "Close");

        helper.waitForCallback(0);
        Assert.assertEquals(1, processedTabs.size());
        Assert.assertEquals(mTabs.get(2), processedTabs.get(0));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoActionView_OneActionDisabled() throws Exception {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.IF_ROOM, ButtonType.TEXT,
                    IconPosition.START, R.string.tab_suggestion_close_tab_action_button, null));
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_group_menu_item, ShowMode.IF_ROOM, ButtonType.ICON,
                    IconPosition.END, R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });
        changeSelectionStateAndAssert(Arrays.asList(new Integer[] {TAB_ID_1}), actions);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { actions.get(0).setShouldEnableAction(false); });
        changeSelectionStateAndAssert(new ArrayList<Integer>(), actions);
        mRenderTestRule.render(mToolbar, "twoActionToolbarPartlyDisabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testActionViewAndMenuItem_Enabled() throws Exception {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.MENU_ONLY, ButtonType.TEXT,
                    IconPosition.START, R.string.tab_suggestion_close_tab_action_button, null));
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_group_menu_item, ShowMode.IF_ROOM, ButtonType.ICON,
                    IconPosition.START, R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });

        changeSelectionStateAndAssert(Arrays.asList(new Integer[] {TAB_ID_2}), actions);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.showOverflowMenu();
            Assert.assertTrue(mToolbar.getMenu()
                                      .findItem(R.id.tab_selection_editor_close_menu_item)
                                      .isVisible());
            Assert.assertTrue(mToolbar.getMenu()
                                      .findItem(R.id.tab_selection_editor_close_menu_item)
                                      .isEnabled());
            mToolbar.hideOverflowMenu();
        });

        mRenderTestRule.render(mToolbar, "oneActionToolbarOneMenuItemEnabled");
    }

    @Test
    @MediumTest
    public void testTwoMenuItems_OneMenuItemDisabled() {
        List<FakeTabSelectionEditorAction> actions = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_close_menu_item, ShowMode.MENU_ONLY, ButtonType.TEXT,
                    IconPosition.END, R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            actions.add(new FakeTabSelectionEditorAction(getActivity(),
                    R.id.tab_selection_editor_group_menu_item, ShowMode.MENU_ONLY, ButtonType.ICON,
                    IconPosition.START, R.string.tab_suggestion_close_tab_action_button,
                    R.drawable.ic_group_icon_16dp));
            configureMenuWithActions(actions);
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { actions.get(1).setShouldEnableAction(false); });
        changeSelectionStateAndAssert(Arrays.asList(new Integer[] {TAB_ID_1}), actions);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbar.showOverflowMenu();
            Assert.assertTrue(mToolbar.getMenu()
                                      .findItem(R.id.tab_selection_editor_close_menu_item)
                                      .isVisible());
            Assert.assertTrue(mToolbar.getMenu()
                                      .findItem(R.id.tab_selection_editor_close_menu_item)
                                      .isEnabled());
            Assert.assertTrue(mToolbar.getMenu()
                                      .findItem(R.id.tab_selection_editor_group_menu_item)
                                      .isVisible());
            Assert.assertFalse(mToolbar.getMenu()
                                       .findItem(R.id.tab_selection_editor_group_menu_item)
                                       .isEnabled());
            mToolbar.hideOverflowMenu();
        });
    }

    private void clickActionView(int id) {
        onView(withId(id)).check(matches(allOf(isDisplayed(), isEnabled())));
        // On Android 12 perform(click()) sometimes fails to trigger the click so force the click on
        // the view object instead.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mToolbar.findViewById(id).performClick(); });
    }

    private void clickMenuItem(int id, String text) {
        onView(withText(text)).check(matches(allOf(isDisplayed(), isEnabled())));
        // On Android 12 perform(click()) works poorly for this as the menu item is flakily reported
        // as < 90% visible.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mToolbar.getMenu().performIdentifierAction(id, /*flags=*/0); });
    }
}
