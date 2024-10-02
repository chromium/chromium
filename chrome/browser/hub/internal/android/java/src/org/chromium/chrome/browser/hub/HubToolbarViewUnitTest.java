// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SHOW_ACTION_BUTTON_TEXT;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;

import androidx.core.content.ContextCompat;
import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.MediumTest;

import com.google.android.material.tabs.TabLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link HubPaneHostView}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubToolbarViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock Runnable mOnButton;
    @Mock Callback<PaneButtonLookup> mPaneButtonLookupCallback;

    @Captor ArgumentCaptor<PaneButtonLookup> mPaneButtonLookupCaptor;

    private Activity mActivity;
    private HubToolbarView mToolbar;
    private Button mActionButton;
    private TabLayout mPaneSwitcher;
    private FrameLayout mMenuButtonContainer;
    private View mSearchBox;
    private EditText mSearchBoxText;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        mToolbar = (HubToolbarView) inflater.inflate(R.layout.hub_toolbar_layout, null, false);
        mActionButton = mToolbar.findViewById(R.id.toolbar_action_button);
        mPaneSwitcher = mToolbar.findViewById(R.id.pane_switcher);
        mMenuButtonContainer = mToolbar.findViewById(R.id.menu_button_container);
        mSearchBox = mToolbar.findViewById(R.id.search_box);
        mSearchBoxText = mToolbar.findViewById(R.id.search_box_text);
        mActivity.setContentView(mToolbar);

        mPropertyModel = new PropertyModel(HubToolbarProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mPropertyModel, mToolbar, HubToolbarViewBinder::bind);
    }

    private FullButtonData makeTestButtonData() {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.ic_add);
        return new DelegateButtonData(displayButtonData, mOnButton);
    }

    @Test
    @MediumTest
    public void testActionButtonVisibility() {
        FullButtonData fullButtonData = makeTestButtonData();
        assertEquals(View.GONE, mActionButton.getVisibility());

        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        assertEquals(View.VISIBLE, mActionButton.getVisibility());
    }

    @Test
    @MediumTest
    public void testActionButtonText() {
        FullButtonData fullButtonData = makeTestButtonData();
        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        assertTrue(TextUtils.isEmpty(mActionButton.getText()));

        mPropertyModel.set(SHOW_ACTION_BUTTON_TEXT, true);
        assertFalse(TextUtils.isEmpty(mActionButton.getText()));
    }

    @Test
    @MediumTest
    public void testActionButtonCallback() {
        FullButtonData fullButtonData = makeTestButtonData();
        mActionButton.callOnClick();
        verifyNoInteractions(mOnButton);

        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        mActionButton.callOnClick();
        verify(mOnButton).run();
    }

    @Test
    @MediumTest
    public void testPaneSwitcherVisibility() {
        mPropertyModel.set(PANE_SWITCHER_BUTTON_DATA, Arrays.asList());
        assertEquals(View.GONE, mPaneSwitcher.getVisibility());

        FullButtonData fullButtonData = makeTestButtonData();
        mPropertyModel.set(PANE_SWITCHER_BUTTON_DATA, Arrays.asList(fullButtonData));
        assertEquals(View.GONE, mPaneSwitcher.getVisibility());

        mPropertyModel.set(
                PANE_SWITCHER_BUTTON_DATA, Arrays.asList(fullButtonData, fullButtonData));
        assertEquals(View.VISIBLE, mPaneSwitcher.getVisibility());
    }

    @Test
    @MediumTest
    public void testPaneSwitcherCallback() {
        FullButtonData fullButtonData = makeTestButtonData();
        mPropertyModel.set(
                PANE_SWITCHER_BUTTON_DATA, Arrays.asList(fullButtonData, fullButtonData));
        verifyNoInteractions(mOnButton);

        mPropertyModel.set(PANE_SWITCHER_INDEX, 1);
        verifyNoInteractions(mOnButton);

        mPaneSwitcher.getTabAt(1).select();
        verifyNoInteractions(mOnButton);

        mPaneSwitcher.getTabAt(0).select();
        verify(mOnButton).run();

        mPaneSwitcher.getTabAt(0).select();
        verify(mOnButton).run();

        mPaneSwitcher.getTabAt(1).select();
        verify(mOnButton, times(2)).run();
    }

    @Test
    @MediumTest
    public void testPaneSwitcherContentDescription() {
        FullButtonData fullButtonData = makeTestButtonData();
        mPropertyModel.set(
                PANE_SWITCHER_BUTTON_DATA, Arrays.asList(fullButtonData, fullButtonData));

        assertEquals(
                fullButtonData.resolveContentDescription(mActivity),
                mPaneSwitcher.getTabAt(0).getContentDescription());
    }

    @Test
    @MediumTest
    public void testPaneSwitcherIndex() {
        FullButtonData fullButtonData = makeTestButtonData();
        List<FullButtonData> paneSwitcherButtonData = new ArrayList<>();
        paneSwitcherButtonData.add(fullButtonData);
        paneSwitcherButtonData.add(fullButtonData);

        mPropertyModel.set(PANE_SWITCHER_BUTTON_DATA, paneSwitcherButtonData);
        mPropertyModel.set(PANE_SWITCHER_INDEX, 0);
        assertEquals(0, mPaneSwitcher.getSelectedTabPosition());

        mPropertyModel.set(PANE_SWITCHER_INDEX, 1);
        assertEquals(1, mPaneSwitcher.getSelectedTabPosition());
    }

    @Test
    @MediumTest
    public void testMenuButtonVisibility() {
        mPropertyModel.set(MENU_BUTTON_VISIBLE, false);
        assertEquals(View.INVISIBLE, mMenuButtonContainer.getVisibility());

        mPropertyModel.set(MENU_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mMenuButtonContainer.getVisibility());
    }

    @Test
    @MediumTest
    public void testPaneButtonLookupCallback() {
        FullButtonData buttonData1 = makeTestButtonData();
        FullButtonData buttonData2 = makeTestButtonData();
        mPropertyModel.set(PANE_SWITCHER_BUTTON_DATA, Arrays.asList(buttonData1, buttonData2));
        mPropertyModel.set(PANE_BUTTON_LOOKUP_CALLBACK, mPaneButtonLookupCallback);

        verify(mPaneButtonLookupCallback).onResult(mPaneButtonLookupCaptor.capture());
        PaneButtonLookup lookup = mPaneButtonLookupCaptor.getValue();

        assertEquals(lookup.get(0), lookup.get(0));
        assertEquals(lookup.get(1), lookup.get(1));
        assertNotEquals(lookup.get(0), lookup.get(1));
    }

    @Test
    @MediumTest
    public void testSearchBoxVisibility() {
        // GONE by default (defined in the xml).
        assertEquals(View.GONE, mSearchBox.getVisibility());
        mPropertyModel.set(SEARCH_BOX_VISIBLE, true);
        assertEquals(View.VISIBLE, mSearchBox.getVisibility());
    }

    @Test
    @MediumTest
    public void testSearchBoxListener() {
        CallbackHelper callbackHelper = new CallbackHelper();
        Runnable testListener =
                () -> {
                    callbackHelper.notifyCalled();
                };

        assertEquals(0, callbackHelper.getCallCount());
        mPropertyModel.set(SEARCH_BOX_LISTENER, testListener);
        mSearchBox.performClick();
        assertEquals(1, callbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
    public void testUpdateIncognitoElements() {
        mPropertyModel.set(IS_INCOGNITO, true);
        assertEquals(
                mActivity.getString(R.string.hub_search_empty_hint_incognito),
                mSearchBoxText.getHint());

        mPropertyModel.set(IS_INCOGNITO, false);
        assertEquals(mActivity.getString(R.string.hub_search_empty_hint), mSearchBoxText.getHint());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
    public void testUpdateSearchBoxColorScheme() {
        mPropertyModel.set(COLOR_SCHEME, HubColorScheme.INCOGNITO);
        assertEquals(
                ContextCompat.getColor(mActivity, R.color.baseline_neutral_60),
                mSearchBoxText.getCurrentHintTextColor());

        GradientDrawable backgroundDrawable = (GradientDrawable) mSearchBox.getBackground();
        assertEquals(
                ColorStateList.valueOf(
                        ContextCompat.getColor(mActivity, R.color.baseline_neutral_20)),
                backgroundDrawable.getColor());

        mPropertyModel.set(COLOR_SCHEME, HubColorScheme.DEFAULT);
        assertEquals(
                SemanticColorUtils.getDefaultTextColor(mActivity),
                mSearchBoxText.getCurrentHintTextColor());
        assertEquals(
                ColorStateList.valueOf(
                        ContextCompat.getColor(mActivity, R.color.color_primary_with_alpha_15)),
                backgroundDrawable.getColor());
    }
}
