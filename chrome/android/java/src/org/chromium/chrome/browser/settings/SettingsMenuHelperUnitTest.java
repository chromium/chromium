// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ImageButton;
import android.widget.ImageView;

import androidx.appcompat.widget.SearchView;
import androidx.appcompat.widget.Toolbar;
import androidx.core.view.AccessibilityDelegateCompat;
import androidx.core.view.ViewCompat;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;
import androidx.fragment.app.Fragment;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.components.browser_ui.settings.SearchViewProvider;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SettingsMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsMenuHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SettingsMenuHelper.Delegate mDelegate;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private MultiColumnSettings mMultiColumnSettings;

    private TestActivity mActivity;

    // Some tests require a real (non-mock) Toolbar.
    private Toolbar mToolbar;

    @Before
    public void setUp() {
        mActivityScenarios.getScenario().onActivity(activity -> mActivity = activity);
        mToolbar = new Toolbar(mActivity);
        when(mDelegate.getHelpAndFeedbackLauncher()).thenReturn(mHelpAndFeedbackLauncher);
    }

    @Test
    public void testCreateOptionsMenu() {
        Menu menu = mock(Menu.class);
        MenuItem menuItem = mock(MenuItem.class);
        when(menu.add(
                        eq(Menu.NONE),
                        eq(R.id.menu_id_general_help),
                        eq(Menu.CATEGORY_SECONDARY),
                        any(Integer.class)))
                .thenReturn(menuItem);

        SettingsMenuHelper.onCreateOptionsMenu(menu, mActivity, mDelegate);

        verify(menu)
                .add(
                        eq(Menu.NONE),
                        eq(R.id.menu_id_general_help),
                        eq(Menu.CATEGORY_SECONDARY),
                        any(Integer.class));
        verify(menuItem).setIcon(any());
    }

    @Test
    public void testCreateOptionsMenu_shownInTab() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        Menu menu = mock(Menu.class);

        SettingsMenuHelper.onCreateOptionsMenu(menu, mActivity, mDelegate);

        verify(menu, never()).add(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void testPrepareOptionsMenu() {
        Menu menu = mock(Menu.class);
        MenuItem menuItem = mock(MenuItem.class);
        when(menu.size()).thenReturn(1);
        when(menu.getItem(0)).thenReturn(menuItem);
        when(menuItem.getIcon()).thenReturn(mock(Drawable.class));

        SettingsMenuHelper.onPrepareOptionsMenu(menu, mDelegate);

        verify(menuItem).setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
    }

    public static class TestMenuFragment extends Fragment {
        boolean mCreateOptionsMenuCalled;
        boolean mPrepareOptionsMenuCalled;

        public TestMenuFragment(boolean hasOptionsMenu) {
            setHasOptionsMenu(hasOptionsMenu);
        }

        @Override
        public void onCreateOptionsMenu(Menu menu, android.view.MenuInflater inflater) {
            mCreateOptionsMenuCalled = true;
            MenuItem item = menu.add(Menu.NONE, 999, Menu.NONE, "Test Item");
            item.setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }

        @Override
        public void onPrepareOptionsMenu(Menu menu) {
            mPrepareOptionsMenuCalled = true;
        }
    }

    @Test
    public void testUpdateOptionsMenu_NoFragment() {
        when(mDelegate.getMainFragment()).thenReturn(null);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        Menu menu = mToolbar.getMenu();
        assertEquals(1, menu.size());
        assertNotNull(menu.findItem(R.id.menu_id_general_help));
    }

    @Test
    public void testUpdateOptionsMenu_FragmentWithoutOptionsMenu() {
        TestMenuFragment fragment = new TestMenuFragment(false);
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "no_menu")
                .commitNow();
        when(mDelegate.getMainFragment()).thenReturn(fragment);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        Menu menu = mToolbar.getMenu();
        assertEquals(1, menu.size());
        assertNotNull(menu.findItem(R.id.menu_id_general_help));
        assertFalse(fragment.mCreateOptionsMenuCalled);
        assertFalse(fragment.mPrepareOptionsMenuCalled);
    }

    @Test
    public void testUpdateOptionsMenu_FragmentWithOptionsMenu() {
        TestMenuFragment fragment = new TestMenuFragment(true);
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "with_menu")
                .commitNow();
        when(mDelegate.getMainFragment()).thenReturn(fragment);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        Menu menu = mToolbar.getMenu();
        assertNotNull(menu.findItem(999));
        assertTrue(fragment.mCreateOptionsMenuCalled);
        assertTrue(fragment.mPrepareOptionsMenuCalled);
    }

    @Test
    public void testUpdateOptionsMenu_FragmentNotAdded() {
        TestMenuFragment fragment = new TestMenuFragment(true);
        when(mDelegate.getMainFragment()).thenReturn(fragment);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        assertFalse(fragment.mCreateOptionsMenuCalled);
    }

    @Test
    public void testUpdateOptionsMenu_shownInTab_FragmentWithOptionsMenu() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        TestMenuFragment fragment = new TestMenuFragment(true);
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "with_menu")
                .commitNow();
        when(mDelegate.getMainFragment()).thenReturn(fragment);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        Menu menu = mToolbar.getMenu();
        assertEquals(1, menu.size());
        assertNotNull(menu.findItem(999));
        assertNull(menu.findItem(R.id.menu_id_general_help));
        assertTrue(fragment.mCreateOptionsMenuCalled);
        assertTrue(fragment.mPrepareOptionsMenuCalled);
    }

    @Test
    public void testUpdateOptionsMenu_shownInTab_FragmentWithoutOptionsMenu() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        TestMenuFragment fragment = new TestMenuFragment(false);
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "no_menu")
                .commitNow();
        when(mDelegate.getMainFragment()).thenReturn(fragment);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        Menu menu = mToolbar.getMenu();
        assertEquals(0, menu.size());
        assertNull(menu.findItem(R.id.menu_id_general_help));
        assertFalse(fragment.mCreateOptionsMenuCalled);
        assertFalse(fragment.mPrepareOptionsMenuCalled);
    }

    @Test
    public void testUpdateOptionsMenu_shownInTab_NoFragment() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        when(mDelegate.getMainFragment()).thenReturn(null);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        Menu menu = mToolbar.getMenu();
        assertEquals(0, menu.size());
        assertNull(menu.findItem(R.id.menu_id_general_help));
    }

    public static class TestTargetedHelpFragment extends Fragment {
        public TestTargetedHelpFragment() {
            setHasOptionsMenu(true);
        }

        @Override
        public void onCreateOptionsMenu(Menu menu, android.view.MenuInflater inflater) {
            menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, "Help");
        }
    }

    public static class TestEditorMenuFragment extends Fragment {
        public TestEditorMenuFragment() {
            setHasOptionsMenu(true);
        }

        @Override
        public void onCreateOptionsMenu(Menu menu, android.view.MenuInflater inflater) {
            menu.add(Menu.NONE, R.id.delete_menu_id, Menu.NONE, "Delete");
            menu.add(Menu.NONE, R.id.help_menu_id, Menu.NONE, "Help");
        }
    }

    @Test
    public void testUpdateOptionsMenu_shownInTab_FragmentWithTargetedHelp_removesHelp() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        TestTargetedHelpFragment fragment = new TestTargetedHelpFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "targeted_help")
                .commitNow();
        when(mDelegate.getMainFragment()).thenReturn(fragment);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        Menu menu = mToolbar.getMenu();
        assertEquals(0, menu.size());
        assertNull(menu.findItem(R.id.menu_id_targeted_help));
        assertNull(menu.findItem(R.id.menu_id_general_help));
    }

    @Test
    public void
            testUpdateOptionsMenu_shownInTab_FragmentWithEditorMenu_removesHelpAndKeepsDelete() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        TestEditorMenuFragment fragment = new TestEditorMenuFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "editor_menu")
                .commitNow();
        when(mDelegate.getMainFragment()).thenReturn(fragment);

        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        Menu menu = mToolbar.getMenu();
        assertEquals(1, menu.size());
        assertNotNull(menu.findItem(R.id.delete_menu_id));
        assertNull(menu.findItem(R.id.help_menu_id));
        assertNull(menu.findItem(R.id.menu_id_general_help));
    }

    /** Mimics a site settings page, which adds both a search item and a help item. */
    public static class TestSiteSettingsMenuFragment extends Fragment
            implements SearchViewProvider {
        public TestSiteSettingsMenuFragment() {
            setHasOptionsMenu(true);
        }

        @Override
        public void onCreateOptionsMenu(Menu menu, android.view.MenuInflater inflater) {
            MenuItem search = menu.add(Menu.NONE, R.id.search, Menu.NONE, "Search");
            search.setActionView(new SearchView(requireContext()));
            menu.add(Menu.NONE, R.id.menu_id_site_settings_help, Menu.NONE, "Help");
        }

        @Override
        public void setSearchViewObserver(SearchViewProvider.Observer observer) {}

        @Override
        public void initSearchView(SearchView searchView) {}
    }

    private TestSiteSettingsMenuFragment addSiteSettingsMenuFragment() {
        TestSiteSettingsMenuFragment fragment = new TestSiteSettingsMenuFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(fragment, "site_settings_menu")
                .commitNow();
        when(mDelegate.getMainFragment()).thenReturn(fragment);
        return fragment;
    }

    @Test
    public void testUpdateOptionsMenu_shownInTab_TwoColumn_removesSearchAndHelp() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        // Show a page that adds its own search and help menu items.
        addSiteSettingsMenuFragment();

        // The detailed page title hosts the fragment's search UI in two-column layouts.
        when(mMultiColumnSettings.isTwoColumn()).thenReturn(true);
        when(mDelegate.getMultiColumnSettings()).thenReturn(mMultiColumnSettings);

        // Build the toolbar menu for the page.
        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        // Neither icon should appear next to the main search box in the toolbar.
        Menu menu = mToolbar.getMenu();
        assertEquals(0, menu.size());
        assertNull(menu.findItem(R.id.search));
        assertNull(menu.findItem(R.id.menu_id_site_settings_help));
    }

    @Test
    public void testUpdateOptionsMenu_shownInTab_SingleColumn_keepsSearchRemovesHelp() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        // Show a page that adds its own search and help menu items.
        addSiteSettingsMenuFragment();

        // In single-column layouts the detailed page title is hidden, so the toolbar search item
        // is the only way to search within the page.
        when(mMultiColumnSettings.isTwoColumn()).thenReturn(false);
        when(mDelegate.getMultiColumnSettings()).thenReturn(mMultiColumnSettings);

        // Build the toolbar menu for the page.
        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        // The search item survives, but settings in a tab never shows a help icon.
        Menu menu = mToolbar.getMenu();
        assertEquals(1, menu.size());
        assertNotNull(menu.findItem(R.id.search));
        assertNull(menu.findItem(R.id.menu_id_site_settings_help));
    }

    @Test
    public void testUpdateOptionsMenu_notShownInTab_keepsSearchAndHelp() {
        // Show a page that adds its own search and help menu items.
        addSiteSettingsMenuFragment();

        // Two-column layout, but settings is not shown in a tab.
        when(mMultiColumnSettings.isTwoColumn()).thenReturn(true);
        when(mDelegate.getMultiColumnSettings()).thenReturn(mMultiColumnSettings);

        // Build the toolbar menu for the page.
        SettingsMenuHelper.updateOptionsMenu(mToolbar, mActivity, mDelegate);

        // Legacy settings keeps the page's menu items and the general help item.
        Menu menu = mToolbar.getMenu();
        assertNotNull(menu.findItem(R.id.search));
        assertNotNull(menu.findItem(R.id.menu_id_site_settings_help));
        assertNotNull(menu.findItem(R.id.menu_id_general_help));
    }

    @Test
    public void testOnPrepareOptionsMenu_shownInTab_removesHelpMenuItems() {
        when(mDelegate.isShownInTab()).thenReturn(true);
        Menu menu = mToolbar.getMenu();
        menu.clear();
        menu.add(Menu.NONE, R.id.menu_id_general_help, Menu.NONE, "General Help");
        menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, "Targeted Help");
        menu.add(Menu.NONE, R.id.help_menu_id, Menu.NONE, "Help");
        menu.add(Menu.NONE, R.id.delete_menu_id, Menu.NONE, "Delete");

        SettingsMenuHelper.onPrepareOptionsMenu(menu, mDelegate);

        assertEquals(1, menu.size());
        assertNotNull(menu.findItem(R.id.delete_menu_id));
        assertNull(menu.findItem(R.id.menu_id_general_help));
        assertNull(menu.findItem(R.id.menu_id_targeted_help));
        assertNull(menu.findItem(R.id.help_menu_id));
    }

    @Test
    public void testPrepareOptionsMenu_MultipleItems() {
        Menu menu = mock(Menu.class);
        MenuItem itemWithIcon = mock(MenuItem.class);
        MenuItem itemWithoutIcon = mock(MenuItem.class);

        when(menu.size()).thenReturn(2);
        when(menu.getItem(0)).thenReturn(itemWithIcon);
        when(menu.getItem(1)).thenReturn(itemWithoutIcon);
        when(itemWithIcon.getIcon()).thenReturn(mock(Drawable.class));
        when(itemWithoutIcon.getIcon()).thenReturn(null);

        SettingsMenuHelper.onPrepareOptionsMenu(menu, mDelegate);

        verify(itemWithIcon).setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        verify(itemWithoutIcon, never()).setShowAsAction(anyInt());
    }

    @Test
    public void testOptionsItemSelected_FragmentHandles() {
        MenuItem item = mock(MenuItem.class);
        Fragment fragment = mock(Fragment.class);
        when(mDelegate.getMainFragment()).thenReturn(fragment);
        when(fragment.onOptionsItemSelected(item)).thenReturn(true);

        assertTrue(SettingsMenuHelper.onOptionsItemSelected(item, mActivity, mDelegate));
    }

    @Test
    public void testOptionsItemSelected_HomeTwoColumn() {
        MenuItem item = mock(MenuItem.class);
        when(item.getItemId()).thenReturn(android.R.id.home);
        MultiColumnSettings multiColumnSettings = mock(MultiColumnSettings.class);
        when(mDelegate.getMultiColumnSettings()).thenReturn(multiColumnSettings);
        when(multiColumnSettings.isTwoColumn()).thenReturn(true);

        assertTrue(SettingsMenuHelper.onOptionsItemSelected(item, mActivity, mDelegate));
        verify(mDelegate).finishSettings();
    }

    @Test
    public void testOptionsItemSelected_HomeSingleColumn() {
        MenuItem item = mock(MenuItem.class);
        when(item.getItemId()).thenReturn(android.R.id.home);
        MultiColumnSettings multiColumnSettings = mock(MultiColumnSettings.class);
        when(mDelegate.getMultiColumnSettings()).thenReturn(multiColumnSettings);
        when(multiColumnSettings.isTwoColumn()).thenReturn(false);

        assertTrue(SettingsMenuHelper.onOptionsItemSelected(item, mActivity, mDelegate));
        verify(mDelegate).onBackPressed();
    }

    @Test
    public void testOptionsItemSelected_HomeNoMultiColumnSearchHandlesBack() {
        MenuItem item = mock(MenuItem.class);
        when(item.getItemId()).thenReturn(android.R.id.home);
        when(mDelegate.getMultiColumnSettings()).thenReturn(null);
        SettingsSearchCoordinator searchCoordinator = mock(SettingsSearchCoordinator.class);
        when(mDelegate.getSearchCoordinator()).thenReturn(searchCoordinator);
        when(searchCoordinator.handleBackAction()).thenReturn(true);

        assertTrue(SettingsMenuHelper.onOptionsItemSelected(item, mActivity, mDelegate));
        verify(mDelegate, never()).finishSettings();
        verify(mDelegate, never()).onBackPressed();
        verify(mDelegate, never()).finishCurrentSettings(any());
    }

    @Test
    public void testOptionsItemSelected_HomeNoMultiColumnSearchDoesNotHandleBack() {
        MenuItem item = mock(MenuItem.class);
        when(item.getItemId()).thenReturn(android.R.id.home);
        when(mDelegate.getMultiColumnSettings()).thenReturn(null);
        SettingsSearchCoordinator searchCoordinator = mock(SettingsSearchCoordinator.class);
        when(mDelegate.getSearchCoordinator()).thenReturn(searchCoordinator);
        when(searchCoordinator.handleBackAction()).thenReturn(false);
        Fragment fragment = mock(Fragment.class);
        when(mDelegate.getMainFragment()).thenReturn(fragment);

        assertTrue(SettingsMenuHelper.onOptionsItemSelected(item, mActivity, mDelegate));
        verify(mDelegate).finishCurrentSettings(fragment);
    }

    @Test
    public void testOptionsItemSelected_GeneralHelp() {
        MenuItem item = mock(MenuItem.class);
        when(item.getItemId()).thenReturn(R.id.menu_id_general_help);

        assertTrue(SettingsMenuHelper.onOptionsItemSelected(item, mActivity, mDelegate));
        verify(mHelpAndFeedbackLauncher).show(eq(mActivity), any(String.class), eq(null));
    }

    @Test
    public void testOptionsItemSelected_UnhandledItem() {
        MenuItem item = mock(MenuItem.class);
        when(item.getItemId()).thenReturn(12345);

        assertFalse(SettingsMenuHelper.onOptionsItemSelected(item, mActivity, mDelegate));
    }

    @Test
    public void testUpdateNavigationIcon_ShowMultiColumn() {
        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                mActivity,
                /* shownInTab= */ false,
                /* show= */ true,
                /* isMultiColumn= */ true,
                /* isMainSettings= */ true);

        assertEquals(
                R.drawable.app_icon_32dp,
                shadowOf(mToolbar.getNavigationIcon()).getCreatedFromResId());
        View navigationButton = getNavigationButton();
        assertNotNull(navigationButton);
        assertFalse(navigationButton.isClickable());
        assertFalse(navigationButton.hasOnClickListeners());
    }

    @Test
    public void testUpdateNavigationIcon_ShowSingleColumn() {
        Activity activity = mock(Activity.class);

        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                activity,
                /* shownInTab= */ false,
                /* show= */ true,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ false);

        assertEquals(
                R.drawable.ic_arrow_back_24dp,
                shadowOf(mToolbar.getNavigationIcon()).getCreatedFromResId());
        View navigationButton = getNavigationButton();
        assertNotNull(navigationButton);
        assertTrue(navigationButton.isClickable());
        assertTrue(navigationButton.hasOnClickListeners());

        navigationButton.performClick();
        verify(activity).onBackPressed();
    }

    @Test
    public void testUpdateNavigationIcon_ShowSingleColumn_shownInTabMainSettings() {
        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                mActivity,
                /* shownInTab= */ true,
                /* show= */ true,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ true);

        assertEquals(
                R.drawable.app_icon_32dp,
                shadowOf(mToolbar.getNavigationIcon()).getCreatedFromResId());
        View navigationButton = getNavigationButton();
        assertNotNull(navigationButton);
        assertFalse(navigationButton.isClickable());
        assertFalse(navigationButton.hasOnClickListeners());
    }

    @Test
    public void testUpdateNavigationIcon_ShowSingleColumn_shownInTabDetailSettings() {
        Activity activity = mock(Activity.class);

        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                activity,
                /* shownInTab= */ true,
                /* show= */ true,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ false);

        assertEquals(
                R.drawable.ic_arrow_back_24dp,
                shadowOf(mToolbar.getNavigationIcon()).getCreatedFromResId());
        View navigationButton = getNavigationButton();
        assertNotNull(navigationButton);
        assertTrue(navigationButton.isClickable());
        assertTrue(navigationButton.hasOnClickListeners());
        assertNotNull(shadowOf(navigationButton).getOnClickListener());

        navigationButton.performClick();
        verify(activity).onBackPressed();
    }

    @Test
    public void testUpdateNavigationIcon_Hide() {
        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                mActivity,
                /* shownInTab= */ false,
                /* show= */ false,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ false);

        assertNull(mToolbar.getNavigationIcon());
    }

    @Test
    public void testUpdateNavigationIcon_LogoAccessibility() {
        // Update the navigation icon to be the Chrome logo.
        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                mActivity,
                /* shownInTab= */ true,
                /* show= */ true,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ true);

        // The navigation button should be reported as an image view for screen readers.
        View navigationButton = getNavigationButton();
        assertFalse(navigationButton.isClickable());
        AccessibilityDelegateCompat delegate =
                ViewCompat.getAccessibilityDelegate(navigationButton);
        assertNotNull(delegate);
        AccessibilityNodeInfoCompat info = AccessibilityNodeInfoCompat.obtain();
        delegate.onInitializeAccessibilityNodeInfo(navigationButton, info);
        assertEquals(ImageView.class.getName(), info.getClassName());
        assertEquals(
                mActivity.getString(R.string.app_name), navigationButton.getContentDescription());
    }

    @Test
    public void testUpdateNavigationIcon_BackButtonAccessibility() {
        // Update the navigation icon to be a back button.
        updateBackButtonInTab();

        // The navigation button should be a clickable back button for screen readers.
        View navigationButton = getNavigationButton();
        assertNotNull(navigationButton);
        assertTrue(navigationButton.isClickable());
        assertTrue(navigationButton.isFocusable());
        assertNull(ViewCompat.getAccessibilityDelegate(navigationButton));
        assertEquals(mActivity.getString(R.string.back), navigationButton.getContentDescription());
    }

    @Test
    public void testUpdateNavigationIcon_BackButtonFocus_shownInTab() {
        mActivity.setContentView(mToolbar);

        updateBackButtonInTab();

        // requestFocus() and the accessibility focus action are sent together, so view focus
        // stands in for screen reader focus in these tests.
        View navigationButton = getNavigationButton();
        assertNotNull(navigationButton);
        assertTrue(navigationButton.isFocusable());
        assertTrue(navigationButton.isFocused());
    }

    @Test
    public void testUpdateNavigationIcon_BackButton_DoesNotFocusWhenAlreadyBackButton() {
        mActivity.setContentView(mToolbar);

        updateBackButtonInTab();

        View navigationButton = getNavigationButton();
        assertNotNull(navigationButton);
        assertTrue(navigationButton.isFocused());

        // Focus another view to simulate user moving focus elsewhere (e.g. a preference item).
        View otherView = new View(mActivity);
        otherView.setFocusable(true);
        otherView.setFocusableInTouchMode(true);
        mToolbar.addView(otherView);
        otherView.requestFocus();
        assertFalse(navigationButton.isFocused());

        // Calling updateNavigationIcon again when already showing the back button should not
        // steal focus back to the navigation button.
        updateBackButtonInTab();

        assertFalse(navigationButton.isFocused());
    }

    @Test
    public void testUpdateNavigationIcon_BackButton_DoesNotFocusWhenToolbarNotOnScreen() {
        // Regression test for crbug.com/556140901: the toolbar is built before it is attached to
        // the window when the Activity is recreated (e.g. after a theme change). Showing the back
        // button then must not force focus onto it, so the screen reader can decide where to go.
        updateBackButtonInTab();

        View navigationButton = getNavigationButton();
        assertNotNull(navigationButton);
        assertEquals(mActivity.getString(R.string.back), navigationButton.getContentDescription());
        assertFalse(navigationButton.isFocused());

        // Attaching the toolbar later and updating again must not focus it either, because the
        // back button is already displayed.
        mActivity.setContentView(mToolbar);
        updateBackButtonInTab();

        assertFalse(navigationButton.isFocused());
    }

    /**
     * Shows the toolbar back button for settings in a tab, that is, single-column layout on a
     * subpage rather than top-level main settings.
     */
    private void updateBackButtonInTab() {
        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                mActivity,
                /* shownInTab= */ true,
                /* show= */ true,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ false);
    }

    /** Returns the navigation button on the toolbar. */
    private View getNavigationButton() {
        for (int i = 0; i < mToolbar.getChildCount(); i++) {
            View child = mToolbar.getChildAt(i);
            if (child instanceof ImageButton) {
                return child;
            }
        }
        throw new IllegalStateException("No navigation button found.");
    }
}
