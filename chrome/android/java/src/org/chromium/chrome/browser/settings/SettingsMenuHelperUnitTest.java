// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.search.SettingsSearchCoordinator;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SettingsMenuHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SettingsMenuHelperUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SettingsMenuHelper.Delegate mDelegate;
    @Mock private Toolbar mToolbar;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarios.getScenario().onActivity(activity -> mActivity = activity);
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

        SettingsMenuHelper.onCreateOptionsMenu(menu, mActivity);

        verify(menu)
                .add(
                        eq(Menu.NONE),
                        eq(R.id.menu_id_general_help),
                        eq(Menu.CATEGORY_SECONDARY),
                        any(Integer.class));
        verify(menuItem).setIcon(any());
    }

    @Test
    public void testPrepareOptionsMenu() {
        Menu menu = mock(Menu.class);
        MenuItem menuItem = mock(MenuItem.class);
        when(menu.size()).thenReturn(1);
        when(menu.getItem(0)).thenReturn(menuItem);
        when(menuItem.getIcon()).thenReturn(mock(android.graphics.drawable.Drawable.class));

        SettingsMenuHelper.onPrepareOptionsMenu(menu);

        verify(menuItem).setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
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
                /* show= */ true,
                /* isMultiColumn= */ true,
                /* isMainSettings= */ true);

        verify(mToolbar).setNavigationIcon(R.drawable.app_icon_32dp);
        verify(mToolbar).setNavigationOnClickListener(null);
    }

    @Test
    public void testUpdateNavigationIcon_ShowSingleColumn() {
        Activity activity = mock(Activity.class);

        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                activity,
                /* show= */ true,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ false);

        verify(mToolbar).setNavigationIcon(R.drawable.ic_arrow_back_24dp);
        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(mToolbar).setNavigationOnClickListener(listenerCaptor.capture());

        listenerCaptor.getValue().onClick(null);
        verify(activity).onBackPressed();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testUpdateNavigationIcon_ShowSingleColumn_SettingsInTabMainSettings() {
        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                mActivity,
                /* show= */ true,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ true);

        verify(mToolbar).setNavigationIcon(R.drawable.app_icon_32dp);
        verify(mToolbar).setNavigationOnClickListener(null);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    @Config(qualifiers = "sw600dp")
    public void testUpdateNavigationIcon_ShowSingleColumn_SettingsInTabDetailSettings() {
        Activity activity = mock(Activity.class);

        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                activity,
                /* show= */ true,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ false);

        verify(mToolbar).setNavigationIcon(R.drawable.ic_arrow_back_24dp);
        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(mToolbar).setNavigationOnClickListener(listenerCaptor.capture());

        listenerCaptor.getValue().onClick(null);
        verify(activity).onBackPressed();
    }

    @Test
    public void testUpdateNavigationIcon_Hide() {
        SettingsMenuHelper.updateNavigationIcon(
                mToolbar,
                mActivity,
                /* show= */ false,
                /* isMultiColumn= */ false,
                /* isMainSettings= */ false);

        verify(mToolbar).setNavigationIcon(null);
    }
}
