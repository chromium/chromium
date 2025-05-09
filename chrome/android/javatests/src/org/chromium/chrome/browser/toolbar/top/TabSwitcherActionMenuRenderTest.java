// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.appcompat.content.res.AppCompatResources;
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
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/** Render tests for tab switcher long-press menu popup. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class TabSwitcherActionMenuRenderTest {
    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER)
                    .build();

    @Mock private Profile mProfile;
    @Mock private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mModel;

    private View mView;

    public TabSwitcherActionMenuRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        ProfileManager.setLastUsedProfileForTesting(mProfile);

        mActivityTestRule.launchActivity(null);

        when(mTabModelSelectorSupplier.hasValue()).thenReturn(true);
        when(mTabModelSelectorSupplier.get()).thenReturn(mTabModelSelector);
        when(mTabModelSelector.getModel(true)).thenReturn(mModel);
        when(mModel.getCount()).thenReturn(0);
    }

    @After
    public void tearDown() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_TabSwitcherActionMenu() throws IOException {
        IncognitoUtils.setEnabledForTesting(true);
        showMenu();
        mRenderTestRule.render(mView, "tab_switcher_action_menu");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRender_TabSwitcherActionMenu_IncognitoDisabled() throws IOException {
        IncognitoUtils.setEnabledForTesting(false);
        showMenu();
        mRenderTestRule.render(mView, "tab_switcher_action_menu_incognito_disabled");
    }

    private void showMenu() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    TabSwitcherActionMenuCoordinator coordinator =
                            new TabSwitcherActionMenuCoordinator(
                                    mProfile, mTabModelSelectorSupplier);

                    coordinator.displayMenu(
                            activity,
                            new ListMenuButton(activity, null),
                            coordinator.buildMenuItems(),
                            null);

                    mView = coordinator.getContentView();
                    if (mView.getParent() != null) {
                        ((ViewGroup) mView.getParent()).removeView(mView);
                    }

                    int popupWidth =
                            activity.getResources()
                                    .getDimensionPixelSize(R.dimen.tab_switcher_menu_width);
                    mView.setBackground(
                            AppCompatResources.getDrawable(activity, R.drawable.menu_bg_tinted));
                    activity.setContentView(mView, new LayoutParams(popupWidth, WRAP_CONTENT));
                });
    }
}
