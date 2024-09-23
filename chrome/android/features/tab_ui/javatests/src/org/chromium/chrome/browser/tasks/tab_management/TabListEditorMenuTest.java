// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.Context;
import android.view.LayoutInflater;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.NumberRollView;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.UiUtils;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/** On-device Unit tests for the {@link TabListEditorMenu} and its related classes. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class TabListEditorMenuTest extends BlankUiTestActivityTestCase {
    private static final int TAB_COUNT = 3;
    private static final Integer TAB_ID_0 = 0;
    private static final Integer TAB_ID_1 = 1;
    private static final Integer TAB_ID_2 = 2;
    private static final Integer[] TAB_IDS = new Integer[] {TAB_ID_0, TAB_ID_1, TAB_ID_2};

    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(5)
                    .setDescription("New selection icons")
                    .build();

    static class FakeTabListEditorAction extends TabListEditorAction {
        private boolean mShouldEnableAction = true;

        FakeTabListEditorAction(
                Context context,
                int menuId,
                @ShowMode int showMode,
                @ButtonType int buttonType,
                @IconPosition int iconPosition,
                int title,
                Integer iconResourceId) {
            super(
                    menuId,
                    showMode,
                    buttonType,
                    iconPosition,
                    title,
                    R.plurals.accessibility_tab_selection_editor_close_tabs,
                    (iconResourceId != null)
                            ? UiUtils.getTintedDrawable(
                                    context, iconResourceId, R.color.default_icon_color_tint_list)
                            : null);
        }

        public void setShouldEnableAction(boolean shouldEnableAction) {
            mShouldEnableAction = shouldEnableAction;
        }

        @Override
        public void onSelectionStateChange(List<Integer> tabs) {
            setEnabledAndItemCount(mShouldEnableAction, tabs.size());
        }

        @Override
        public boolean performAction(List<Tab> tabs) {
            return true;
        }

        @Override
        public boolean shouldHideEditorAfterAction() {
            return false;
        }
    }

    // For R8 optimizer message
    @Mock private Tab mTabDoNotUse;

    // Real mocks.
    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private ActionDelegate mDelegate;

    private List<Tab> mTabs = new ArrayList<>();

    private TabListEditorToolbar mToolbar;
    private TabListEditorMenu mTabListEditorMenu;
    private ListMenuButton mMenuButton;
    private PropertyListModel<PropertyModel, PropertyKey> mPropertyListModel;
    private ListModelChangeProcessor mChangeProcessor;

    public TabListEditorMenuTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getCount()).thenReturn(TAB_COUNT);

        for (int id = 0; id < TAB_COUNT; id++) {
            Tab tab = mock(Tab.class);
            mTabs.add(tab);
            when(tab.getId()).thenReturn(TAB_IDS[id]);
            when(mTabModel.getTabAt(id)).thenReturn(tab);
            when(mTabModel.getTabById(id)).thenReturn(tab);
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionDelegate = new SelectionDelegate<>();
                    mSelectionDelegate.setSelectionModeEnabledForZeroItems(true);
                    LinearLayout layout = new LinearLayout(getActivity());
                    LinearLayout.LayoutParams layoutParams =
                            new LinearLayout.LayoutParams(
                                    LinearLayout.LayoutParams.MATCH_PARENT,
                                    LinearLayout.LayoutParams.MATCH_PARENT);
                    layout.setLayoutParams(layoutParams);

                    LayoutInflater inflater = LayoutInflater.from(getActivity());
                    mToolbar =
                            (TabListEditorToolbar)
                                    inflater.inflate(R.layout.tab_list_editor_toolbar, null);
                    layoutParams =
                            new LinearLayout.LayoutParams(
                                    LinearLayout.LayoutParams.MATCH_PARENT,
                                    getActivity()
                                            .getResources()
                                            .getDimensionPixelSize(
                                                    R.dimen.toolbar_height_no_shadow));
                    layout.addView(mToolbar, layoutParams);
                    getActivity().setContentView(layout);
                    mToolbar.initialize(mSelectionDelegate, 0, 0, 0, true);

                    mPropertyListModel = new PropertyListModel<>();
                    mTabListEditorMenu =
                            new TabListEditorMenu(getActivity(), mToolbar.getActionViewLayout());
                    mMenuButton = mToolbar.getActionViewLayout().getListMenuButtonForTesting();
                    mSelectionDelegate.addObserver(mTabListEditorMenu);
                    mChangeProcessor =
                            new ListModelChangeProcessor(
                                    mPropertyListModel,
                                    mTabListEditorMenu,
                                    new TabListEditorMenuAdapter());
                    mPropertyListModel.addObserver(mChangeProcessor);
                });
    }

    @Override
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyListModel.clear();
                });
        super.tearDownTest();
    }

    private void configureMenuWithActions(List<FakeTabListEditorAction> actions) {
        mPropertyListModel.clear();
        List<PropertyModel> models = new ArrayList<>();
        for (FakeTabListEditorAction action : actions) {
            action.getPropertyModel()
                    .set(
                            TabListEditorActionProperties.TEXT_TINT,
                            AppCompatResources.getColorStateList(
                                    getActivity(), R.color.default_text_color_list));
            action.getPropertyModel()
                    .set(
                            TabListEditorActionProperties.ICON_TINT,
                            AppCompatResources.getColorStateList(
                                    getActivity(), R.color.default_icon_color_tint_list));
            action.configure(
                    () -> mTabGroupModelFilter,
                    mSelectionDelegate,
                    mDelegate,
                    /* editorSupportsActionOnRelatedTabs= */ false);
            models.add(action.getPropertyModel());
        }
        mPropertyListModel.addAll(models, 0);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleActionView_TextAndIcon_Enabled() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END,
                                    R.plurals.tab_selection_editor_close_tabs,
                                    R.drawable.ic_close_tabs_24dp));
                    configureMenuWithActions(actions);
                });

        setSelectedItems(new HashSet<>(Arrays.asList(new Integer[] {TAB_ID_0, TAB_ID_2})));
        assertActionView(R.id.tab_list_editor_close_menu_item, true);

        forceFinishRollAnimation();
        mRenderTestRule.render(mToolbar, "singleActionToolbarEnabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleActionView_TextAndIcon_Disabled() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON_AND_TEXT,
                                    IconPosition.END,
                                    R.plurals.tab_selection_editor_close_tabs,
                                    R.drawable.ic_close_tabs_24dp));
                    configureMenuWithActions(actions);
                });

        ThreadUtils.runOnUiThreadBlocking(() -> actions.get(0).setShouldEnableAction(false));
        setSelectedItems(new HashSet<>(Arrays.asList(new Integer[] {TAB_ID_1})));
        assertActionView(R.id.tab_list_editor_close_menu_item, false);

        forceFinishRollAnimation();
        mRenderTestRule.render(mToolbar, "singleActionToolbarDisabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleActionView_IconOnly_Enabled() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON,
                                    IconPosition.END,
                                    R.plurals.tab_selection_editor_close_tabs,
                                    R.drawable.ic_close_tabs_24dp));
                    configureMenuWithActions(actions);
                });

        setSelectedItems(
                new HashSet<>(Arrays.asList(new Integer[] {TAB_ID_0, TAB_ID_1, TAB_ID_2})));
        assertActionView(R.id.tab_list_editor_close_menu_item, true);

        forceFinishRollAnimation();
        mRenderTestRule.render(mToolbar, "singleActionToolbarIconOnlyEnabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleActionView_Click() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.IF_ROOM,
                                    ButtonType.TEXT,
                                    IconPosition.END,
                                    R.string.tab_selection_editor_select_all,
                                    R.drawable.ic_select_all_24dp));
                    configureMenuWithActions(actions);
                });

        final List<Tab> processedTabs = new ArrayList<>();
        final CallbackHelper helper = new CallbackHelper();
        final ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        processedTabs.clear();
                        processedTabs.addAll(tabs);
                        helper.notifyCalled();
                    }
                };
        setSelectedItems(new HashSet<>(Arrays.asList(new Integer[] {TAB_ID_0, TAB_ID_2})));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.get(0).addActionObserver(observer);
                });

        forceFinishRollAnimation();
        mRenderTestRule.render(mToolbar, "singleActionToolbarTextOnlyEnabled");
        assertActionView(R.id.tab_list_editor_close_menu_item, true);
        clickActionView(R.id.tab_list_editor_close_menu_item);

        helper.waitForCallback(0);
        assertEquals(2, processedTabs.size());
        assertEquals(mTabs.get(0), processedTabs.get(0));
        assertEquals(mTabs.get(2), processedTabs.get(1));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleMenuItem_Disabled() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    R.string.tab_selection_editor_deselect_all,
                                    R.drawable.ic_deselect_all_24dp));
                    configureMenuWithActions(actions);
                });

        ThreadUtils.runOnUiThreadBlocking(() -> actions.get(0).setShouldEnableAction(false));
        setSelectedItems(new HashSet<>(Arrays.asList(new Integer[] {TAB_ID_0, TAB_ID_1})));

        PopupListener listener = new PopupListener();
        openMenu(listener);
        assertMenuItem("Deselect all", false);

        forceFinishRollAnimation();
        mRenderTestRule.render(
                mTabListEditorMenu.getContentView(), "singleMenuItemDisabled_Menu");
        closeMenu(listener);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testSingleMenuItem_Click() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    R.plurals.tab_selection_editor_close_tabs,
                                    R.drawable.ic_close_tabs_24dp));
                    configureMenuWithActions(actions);
                });

        final List<Tab> processedTabs = new ArrayList<>();
        final CallbackHelper helper = new CallbackHelper();
        final ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        processedTabs.clear();
                        processedTabs.addAll(tabs);
                        helper.notifyCalled();
                    }
                };

        setSelectedItems(new HashSet<>(Arrays.asList(new Integer[] {TAB_ID_2})));
        ThreadUtils.runOnUiThreadBlocking(() -> actions.get(0).addActionObserver(observer));

        PopupListener listener = new PopupListener();
        openMenu(listener);
        assertMenuItem("Close tab", true);
        clickMenuItem("Close tab");
        helper.waitForCallback(0);

        forceFinishRollAnimation();
        mRenderTestRule.render(mToolbar, "singleMenuItemEnabled_Toolbar");
        mRenderTestRule.render(
                mTabListEditorMenu.getContentView(), "singleMenuItemEnabled_Menu");
        closeMenu(listener);

        assertEquals(1, processedTabs.size());
        assertEquals(mTabs.get(2), processedTabs.get(0));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoActionView_OneActionDisabled() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON,
                                    IconPosition.START,
                                    R.plurals.tab_selection_editor_close_tabs,
                                    R.drawable.ic_close_tabs_24dp));
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_group_menu_item,
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON,
                                    IconPosition.END,
                                    R.plurals.tab_selection_editor_group_tabs,
                                    R.drawable.ic_widgets));
                    configureMenuWithActions(actions);
                });

        ThreadUtils.runOnUiThreadBlocking(() -> actions.get(0).setShouldEnableAction(false));
        setSelectedItems(new HashSet<>());
        assertActionView(R.id.tab_list_editor_close_menu_item, false);
        assertActionView(R.id.tab_list_editor_group_menu_item, true);

        forceFinishRollAnimation();
        mRenderTestRule.render(mToolbar, "twoActionToolbarPartlyDisabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testActionViewAndMenuItem_Enabled() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    R.plurals.tab_selection_editor_close_tabs,
                                    R.drawable.ic_close_tabs_24dp));
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_group_menu_item,
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON,
                                    IconPosition.START,
                                    R.plurals.tab_selection_editor_group_tabs,
                                    R.drawable.ic_widgets));
                    configureMenuWithActions(actions);
                });

        setSelectedItems(new HashSet<>(Arrays.asList(new Integer[] {TAB_ID_2})));

        assertActionView(R.id.tab_list_editor_group_menu_item, true);

        PopupListener listener = new PopupListener();
        openMenu(listener);
        assertMenuItem("Close tab", true);
        forceFinishRollAnimation();
        mRenderTestRule.render(mToolbar, "oneActionToolbarOneMenuItemEnabled_Toobar");
        mRenderTestRule.render(
                mTabListEditorMenu.getContentView(),
                "oneActionToolbarOneMenuItemEnabled_Menu");
        closeMenu(listener);
    }

    // Regression test for https://crbug.com/1377205.
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testLongTextActionViewAndMenuItem() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NumberRollView numberRoll =
                            (NumberRollView) mToolbar.getActionViewLayout().getChildAt(0);
                    numberRoll.setStringForZero(R.string.close_all_tabs_dialog_message_incognito);
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.START,
                                    R.plurals.tab_selection_editor_close_tabs,
                                    R.drawable.ic_close_tabs_24dp));
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_group_menu_item,
                                    ShowMode.IF_ROOM,
                                    ButtonType.ICON,
                                    IconPosition.START,
                                    R.plurals.tab_selection_editor_group_tabs,
                                    R.drawable.ic_widgets));
                    configureMenuWithActions(actions);
                    actions.get(0).setShouldEnableAction(false);
                    actions.get(1).setShouldEnableAction(false);
                });

        setSelectedItems(new HashSet<>(Arrays.asList(new Integer[] {})));

        assertActionView(R.id.tab_list_editor_group_menu_item, false);

        forceFinishRollAnimation();
        mRenderTestRule.render(mToolbar, "longTextV2ActionAndMenu");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoMenuItems_OneMenuItemDisabled() throws Exception {
        List<FakeTabListEditorAction> actions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_close_menu_item,
                                    ShowMode.MENU_ONLY,
                                    ButtonType.TEXT,
                                    IconPosition.END,
                                    R.plurals.tab_selection_editor_close_tabs,
                                    R.drawable.ic_close_tabs_24dp));
                    actions.add(
                            new FakeTabListEditorAction(
                                    getActivity(),
                                    R.id.tab_list_editor_group_menu_item,
                                    ShowMode.MENU_ONLY,
                                    ButtonType.ICON,
                                    IconPosition.START,
                                    R.plurals.tab_selection_editor_group_tabs,
                                    R.drawable.ic_widgets));
                    configureMenuWithActions(actions);
                });

        ThreadUtils.runOnUiThreadBlocking(() -> actions.get(1).setShouldEnableAction(false));
        setSelectedItems(new HashSet<>(Arrays.asList(new Integer[] {TAB_ID_1})));

        PopupListener listener = new PopupListener();
        openMenu(listener);
        assertMenuItem("Close tab", true);
        assertMenuItem("Group tab", false);
        forceFinishRollAnimation();
        mRenderTestRule.render(
                mTabListEditorMenu.getContentView(), "twoMenuItemsPartlyDisabled_Menu");
        closeMenu(listener);
    }

    /** Helper for detecting menu shown popup events. */
    static class PopupListener implements ListMenuButton.PopupMenuShownListener {
        private CallbackHelper mShown = new CallbackHelper();
        private CallbackHelper mHidden = new CallbackHelper();

        @Override
        public void onPopupMenuShown() {
            mShown.notifyCalled();
        }

        @Override
        public void onPopupMenuDismissed() {
            mHidden.notifyCalled();
        }

        public void waitForShown() throws TimeoutException {
            mShown.waitForOnly();
        }

        public void waitForHidden() {
            mHidden.notifyCalled();
        }
    }

    private void assertActionView(int id, boolean enabled) {
        onViewWaiting(
                allOf(
                        withId(id),
                        isDescendantOfA(withId(R.id.action_view_layout)),
                        isDisplayed(),
                        enabled ? isEnabled() : not(isEnabled())));
    }

    private void assertMenuItem(String text, boolean enabled) {
        onViewWaiting(
                allOf(
                        withText(text),
                        isDescendantOfA(withId(R.id.app_menu_list)),
                        isDisplayed(),
                        enabled ? isEnabled() : not(isEnabled())));
    }

    private void openMenu(PopupListener listener) throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(() -> mMenuButton.addPopupListener(listener));
        onViewWaiting(allOf(withId(R.id.list_menu_button), isDisplayed(), isEnabled()))
                .perform(click());
        listener.waitForShown();
    }

    private void closeMenu(PopupListener listener) {
        ThreadUtils.runOnUiThreadBlocking(() -> mMenuButton.dismiss());
        listener.waitForHidden();
        ThreadUtils.runOnUiThreadBlocking(() -> mMenuButton.removePopupListener(listener));
    }

    private void clickActionView(int id) {
        onViewWaiting(
                allOf(withId(id), isDescendantOfA(withId(R.id.action_view_layout)), isDisplayed()));
        // On Android 12 perform(click()) sometimes fails to trigger the click so force the click on
        // the view object instead.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mToolbar.findViewById(id).performClick();
                });
    }

    private void clickMenuItem(String text) {
        onViewWaiting(
                        allOf(
                                withText(text),
                                isDescendantOfA(withId(R.id.app_menu_list)),
                                isDisplayed()))
                .perform(click());
    }

    private void setSelectedItems(Set<Integer> tabIds) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSelectionDelegate.setSelectedItems(tabIds);
                    mToolbar.invalidate();
                });
    }

    private void forceFinishRollAnimation() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NumberRollView numberRoll =
                            (NumberRollView) mToolbar.getActionViewLayout().getChildAt(0);
                    numberRoll.endAnimationsForTesting();
                });
    }
}
