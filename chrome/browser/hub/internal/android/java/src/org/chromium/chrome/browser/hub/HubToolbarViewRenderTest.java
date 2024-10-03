// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable;
import org.chromium.chrome.browser.toolbar.TabSwitcherDrawable.TabSwitcherDrawableLocation;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;
import java.util.List;

/** Render tests for {@link HubPaneHostView}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
public class HubToolbarViewRenderTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .setRevision(7)
                    .build();

    @Mock private TabSwitcherDrawable.Observer mTabSwitcherDrawableObserver;

    private Activity mActivity;
    private HubToolbarView mToolbar;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        ThreadUtils.runOnUiThreadBlocking(this::setUpOnUi);
    }

    private void setUpOnUi() {
        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mToolbar = (HubToolbarView) inflater.inflate(R.layout.hub_toolbar_layout, null, false);
        mActivity.setContentView(mToolbar);

        mPropertyModel = new PropertyModel(HubToolbarProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mPropertyModel, mToolbar, HubToolbarViewBinder::bind);
    }

    private FullButtonData enabledButtonData(@DrawableRes int drawableRes) {
        return makeButtonData(drawableRes, () -> {});
    }

    private FullButtonData disabledButtonData(@DrawableRes int drawableRes) {
        return makeButtonData(drawableRes, null);
    }

    private FullButtonData makeButtonData(
            @DrawableRes int drawableRes, @Nullable Runnable onPress) {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, drawableRes);
        return new DelegateButtonData(displayButtonData, onPress);
    }

    private FullButtonData makeButtonData(Drawable drawable, @Nullable Runnable onPress) {
        DisplayButtonData displayButtonData =
                new DrawableButtonData(R.string.button_new_tab, R.string.button_new_tab, drawable);
        return new DelegateButtonData(displayButtonData, onPress);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testActionButton() throws Exception {
        FullButtonData enabledButtonData = enabledButtonData(R.drawable.new_tab_icon);
        FullButtonData disabledButtonData = disabledButtonData(R.drawable.new_tab_icon);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, enabledButtonData);
                    mPropertyModel.set(HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT, true);
                });
        mRenderTestRule.render(mToolbar, "actionButtonWithText");

        ThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT, false));
        mRenderTestRule.render(mToolbar, "actionButtonOnlyImage");

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPropertyModel.set(
                                HubToolbarProperties.ACTION_BUTTON_DATA, disabledButtonData));
        mRenderTestRule.render(mToolbar, "disabledButtonOnlyImage");

        ThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, null));
        mRenderTestRule.render(mToolbar, "noActionButton");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, enabledButtonData);
                    mPropertyModel.set(HubToolbarProperties.MENU_BUTTON_VISIBLE, true);
                    mPropertyModel.set(HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT, true);
                    mPropertyModel.set(HubToolbarProperties.COLOR_SCHEME, HubColorScheme.INCOGNITO);
                });
        mRenderTestRule.render(mToolbar, "actionButtonIncognito");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, disabledButtonData);
                });
        mRenderTestRule.render(mToolbar, "disabledActionButtonIncognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPaneSwitcher() throws Exception {
        FullButtonData actionButtonData = enabledButtonData(R.drawable.new_tab_icon);
        List<FullButtonData> paneSwitcherButtonData = new ArrayList<>();
        paneSwitcherButtonData.add(enabledButtonData(R.drawable.new_tab_icon));
        paneSwitcherButtonData.add(enabledButtonData(R.drawable.incognito_small));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, actionButtonData);
                    mPropertyModel.set(HubToolbarProperties.MENU_BUTTON_VISIBLE, true);
                    mPropertyModel.set(HubToolbarProperties.PANE_SWITCHER_INDEX, 0);
                    mPropertyModel.set(
                            HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA, paneSwitcherButtonData);
                });
        mRenderTestRule.render(mToolbar, "paneSwitcher");

        ThreadUtils.runOnUiThreadBlocking(
                () -> mPropertyModel.set(HubToolbarProperties.PANE_SWITCHER_INDEX, 1));
        mRenderTestRule.render(mToolbar, "paneSwitcherSelectedIndex");

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mPropertyModel.set(
                                HubToolbarProperties.COLOR_SCHEME, HubColorScheme.INCOGNITO));
        mRenderTestRule.render(mToolbar, "paneSwitcherIncognito");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testHideMenuButton() throws Exception {
        FullButtonData actionButtonData = enabledButtonData(R.drawable.new_tab_icon);
        List<FullButtonData> paneSwitcherButtonData = new ArrayList<>();
        paneSwitcherButtonData.add(enabledButtonData(R.drawable.new_tab_icon));
        paneSwitcherButtonData.add(enabledButtonData(R.drawable.incognito_small));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, actionButtonData);
                    mPropertyModel.set(HubToolbarProperties.MENU_BUTTON_VISIBLE, false);
                    mPropertyModel.set(HubToolbarProperties.PANE_SWITCHER_INDEX, 0);
                    mPropertyModel.set(
                            HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA, paneSwitcherButtonData);
                });
        mRenderTestRule.render(mToolbar, "menuButtonHidden");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabSwitcherDrawable_toggleNotificationStatus() throws Exception {
        TabSwitcherDrawable tabSwitcherDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        mActivity,
                        BrandedColorScheme.APP_DEFAULT,
                        TabSwitcherDrawableLocation.HUB_TOOLBAR);
        tabSwitcherDrawable.addTabSwitcherDrawableObserver(mTabSwitcherDrawableObserver);
        tabSwitcherDrawable.updateForTabCount(/* tabCount= */ 1, /* incognito= */ false);
        tabSwitcherDrawable.setNotificationIconStatus(/* shouldShow= */ true);
        verify(mTabSwitcherDrawableObserver, times(2)).onDrawableStateChanged();

        FullButtonData actionButtonData = enabledButtonData(R.drawable.new_tab_icon);
        List<FullButtonData> paneSwitcherButtonData = new ArrayList<>();
        paneSwitcherButtonData.add(makeButtonData(tabSwitcherDrawable, () -> {}));
        paneSwitcherButtonData.add(enabledButtonData(R.drawable.incognito_small));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, actionButtonData);
                    mPropertyModel.set(HubToolbarProperties.MENU_BUTTON_VISIBLE, true);
                    mPropertyModel.set(HubToolbarProperties.PANE_SWITCHER_INDEX, 0);
                    mPropertyModel.set(
                            HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA, paneSwitcherButtonData);
                });
        mRenderTestRule.render(mToolbar, "onGTSTabSwitcherDrawableNotificationOn");

        tabSwitcherDrawable.setNotificationIconStatus(/* shouldShow= */ false);
        verify(mTabSwitcherDrawableObserver, times(3)).onDrawableStateChanged();
        mRenderTestRule.render(mToolbar, "onGTSTabSwitcherDrawableNotificationOff");
        tabSwitcherDrawable.removeTabSwitcherDrawableObserver(mTabSwitcherDrawableObserver);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabSwitcherDrawable_toggleNotificationStatusIncognito() throws Exception {
        TabSwitcherDrawable tabSwitcherDrawable =
                TabSwitcherDrawable.createTabSwitcherDrawable(
                        mActivity,
                        BrandedColorScheme.INCOGNITO,
                        TabSwitcherDrawableLocation.HUB_TOOLBAR);
        tabSwitcherDrawable.addTabSwitcherDrawableObserver(mTabSwitcherDrawableObserver);
        tabSwitcherDrawable.updateForTabCount(/* tabCount= */ 1, /* incognito= */ true);
        tabSwitcherDrawable.setNotificationIconStatus(/* shouldShow= */ true);
        verify(mTabSwitcherDrawableObserver, times(2)).onDrawableStateChanged();

        FullButtonData actionButtonData = enabledButtonData(R.drawable.new_tab_icon);
        List<FullButtonData> paneSwitcherButtonData = new ArrayList<>();
        paneSwitcherButtonData.add(makeButtonData(tabSwitcherDrawable, () -> {}));
        paneSwitcherButtonData.add(enabledButtonData(R.drawable.incognito_small));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, actionButtonData);
                    mPropertyModel.set(HubToolbarProperties.MENU_BUTTON_VISIBLE, true);
                    mPropertyModel.set(HubToolbarProperties.PANE_SWITCHER_INDEX, 1);
                    mPropertyModel.set(
                            HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA, paneSwitcherButtonData);
                    mPropertyModel.set(HubToolbarProperties.COLOR_SCHEME, HubColorScheme.INCOGNITO);
                });
        mRenderTestRule.render(mToolbar, "onIncognitoTabSwitcherDrawableNotificationOn");

        tabSwitcherDrawable.setNotificationIconStatus(/* shouldShow= */ false);
        verify(mTabSwitcherDrawableObserver, times(3)).onDrawableStateChanged();
        mRenderTestRule.render(mToolbar, "onIncognitoTabSwitcherDrawableNotificationOff");
        tabSwitcherDrawable.removeTabSwitcherDrawableObserver(mTabSwitcherDrawableObserver);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
    public void testSearchBox() throws Exception {
        FullButtonData actionButtonData = enabledButtonData(R.drawable.new_tab_icon);
        List<FullButtonData> paneSwitcherButtonData = new ArrayList<>();
        paneSwitcherButtonData.add(enabledButtonData(R.drawable.new_tab_icon));
        paneSwitcherButtonData.add(enabledButtonData(R.drawable.incognito_small));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPropertyModel.set(HubToolbarProperties.ACTION_BUTTON_DATA, actionButtonData);
                    mPropertyModel.set(HubToolbarProperties.MENU_BUTTON_VISIBLE, true);
                    mPropertyModel.set(HubToolbarProperties.PANE_SWITCHER_INDEX, 0);
                    mPropertyModel.set(
                            HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA, paneSwitcherButtonData);
                    mPropertyModel.set(HubToolbarProperties.SEARCH_BOX_VISIBLE, true);
                    mPropertyModel.set(HubToolbarProperties.IS_INCOGNITO, false);
                });
        mRenderTestRule.render(mToolbar, "searchBox");
    }
}
