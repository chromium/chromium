// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.ACTION_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.HUB_SEARCH_ENABLED_STATE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.IS_INCOGNITO;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.MENU_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_BUTTON_LOOKUP_CALLBACK;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_BUTTON_DATA;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.PANE_SWITCHER_INDEX;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LISTENER;
import static org.chromium.chrome.browser.hub.HubToolbarProperties.SEARCH_LOUPE_VISIBLE;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.core.content.ContextCompat;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import com.google.android.material.color.MaterialColors;
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
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubToolbarProperties.PaneButtonLookup;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.util.XrUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/** Unit tests for {@link HubPaneHostView}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
// TODO(crbug.com/419289558): Re-enable color surface feature flags
@Features.DisableFeatures({
    ChromeFeatureList.ANDROID_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_SURFACE_COLOR_UPDATE,
    ChromeFeatureList.GRID_TAB_SWITCHER_UPDATE
})
public class HubToolbarViewUnitTest {
    // All the tests in this file will run twice, once for isXrDevice=true and once for
    // isXrDevice=false. Expect all the tests with the same results on XR devices too.
    // The setup ensures the correct environment is configured for each run.
    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(new Object[][] {{true}, {false}});
    }

    @Parameter(0)
    public boolean mIsXrDevice;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Rule public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Mock Runnable mOnButton;
    @Mock Callback<PaneButtonLookup> mPaneButtonLookupCallback;
    @Mock private Pane mPane;

    @Captor ArgumentCaptor<PaneButtonLookup> mPaneButtonLookupCaptor;

    private ObservableSupplierImpl<Pane> mFocusedPaneSupplier;
    private Activity mActivity;
    private FrameLayout mToolbarContainer;
    private Button mActionButton;
    private TabLayout mPaneSwitcher;
    private LinearLayout mMenuButtonContainer;
    private View mSearchBox;
    private View mSearchLoupe;
    private EditText mSearchBoxText;
    private PropertyModel mPropertyModel;
    private HubColorMixer mColorMixer;

    @Before
    public void setUp() throws Exception {
        XrUtils.setXrDeviceForTesting(mIsXrDevice);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mActivity = activity;
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        LayoutInflater inflater = LayoutInflater.from(mActivity);
        int layoutId = mIsXrDevice ? R.layout.hub_xr_toolbar_layout : R.layout.hub_toolbar_layout;
        mToolbarContainer = (FrameLayout) inflater.inflate(layoutId, null, false);
        mActionButton = mToolbarContainer.findViewById(R.id.toolbar_action_button);
        mPaneSwitcher = mToolbarContainer.findViewById(R.id.pane_switcher);
        mMenuButtonContainer = mToolbarContainer.findViewById(R.id.menu_button_container);
        mSearchBox = mToolbarContainer.findViewById(R.id.search_box);
        mSearchLoupe = mToolbarContainer.findViewById(R.id.search_loupe);
        mSearchBoxText = mToolbarContainer.findViewById(R.id.search_box_text);
        mActivity.setContentView(mToolbarContainer);

        mFocusedPaneSupplier = new ObservableSupplierImpl<>();
        mColorMixer =
                spy(
                        new HubColorMixerImpl(
                                mActivity,
                                new ObservableSupplierImpl<>(true),
                                mFocusedPaneSupplier));
        mPropertyModel =
                new PropertyModel.Builder(HubToolbarProperties.ALL_KEYS)
                        .with(COLOR_MIXER, mColorMixer)
                        .build();
        PropertyModelChangeProcessor.create(
                mPropertyModel,
                mToolbarContainer.findViewById(R.id.hub_toolbar),
                HubToolbarViewBinder::bind);
        when(mPane.getColorScheme()).thenReturn(HubColorScheme.DEFAULT);
        mFocusedPaneSupplier.set(mPane);
    }

    private FullButtonData makeTestButtonData() {
        DisplayButtonData displayButtonData =
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.ic_add);
        return new DelegateButtonData(displayButtonData, mOnButton);
    }

    @Test
    public void testActionButtonVisibility() {
        FullButtonData fullButtonData = makeTestButtonData();
        assertEquals(View.GONE, mActionButton.getVisibility());

        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        assertEquals(View.VISIBLE, mActionButton.getVisibility());
    }

    @Test
    public void testActionButtonCallback() {
        FullButtonData fullButtonData = makeTestButtonData();
        mActionButton.callOnClick();
        verifyNoInteractions(mOnButton);

        mPropertyModel.set(ACTION_BUTTON_DATA, fullButtonData);
        mActionButton.callOnClick();
        verify(mOnButton).run();
    }

    @Test
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
    public void testPaneSwitcherContentDescription() {
        FullButtonData fullButtonData = makeTestButtonData();
        mPropertyModel.set(
                PANE_SWITCHER_BUTTON_DATA, Arrays.asList(fullButtonData, fullButtonData));

        assertEquals(
                fullButtonData.resolveContentDescription(mActivity),
                mPaneSwitcher.getTabAt(0).getContentDescription());
    }

    @Test
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
    public void testMenuButtonVisibility() {
        mPropertyModel.set(MENU_BUTTON_VISIBLE, false);
        assertEquals(View.INVISIBLE, mMenuButtonContainer.getVisibility());

        mPropertyModel.set(MENU_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mMenuButtonContainer.getVisibility());
    }

    @Test
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
    public void testSearchBoxVisibility() {
        // GONE by default (defined in the xml).
        assertEquals(View.GONE, mSearchBox.getVisibility());
        assertEquals(View.GONE, mSearchLoupe.getVisibility());
        mPropertyModel.set(SEARCH_BOX_VISIBLE, true);
        assertEquals(View.VISIBLE, mSearchBox.getVisibility());
        mPropertyModel.set(SEARCH_LOUPE_VISIBLE, true);
        assertEquals(View.VISIBLE, mSearchLoupe.getVisibility());
    }

    @Test
    public void testSearchBoxListener() {
        CallbackHelper callbackHelper = new CallbackHelper();
        Runnable testListener =
                () -> {
                    callbackHelper.notifyCalled();
                };

        assertEquals(0, callbackHelper.getCallCount());
        mPropertyModel.set(SEARCH_LISTENER, testListener);
        mSearchBox.performClick();
        assertEquals(1, callbackHelper.getCallCount());
        mSearchLoupe.performClick();
        assertEquals(2, callbackHelper.getCallCount());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
    public void testUpdateIncognitoElements() {
        mPropertyModel.set(IS_INCOGNITO, true);
        assertEquals(
                mActivity.getString(R.string.hub_search_empty_hint_incognito),
                mSearchBoxText.getHint());

        mPropertyModel.set(IS_INCOGNITO, false);
        assertEquals(mActivity.getString(R.string.hub_search_empty_hint), mSearchBoxText.getHint());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
    public void testUpdateSearchBoxColorScheme() {
        forceSetColorScheme(HubColorScheme.INCOGNITO);
        assertEquals(
                ContextCompat.getColor(mActivity, R.color.default_text_color_secondary_light),
                mSearchBoxText.getCurrentHintTextColor());

        GradientDrawable backgroundDrawable = (GradientDrawable) mSearchBox.getBackground();
        assertEquals(
                ColorStateList.valueOf(
                        ContextCompat.getColor(
                                mActivity, R.color.gm3_baseline_surface_container_highest_dark)),
                backgroundDrawable.getColor());

        forceSetColorScheme(HubColorScheme.DEFAULT);
        assertEquals(
                MaterialColors.getColor(mActivity, R.attr.colorOnSurfaceVariant, "Test"),
                mSearchBoxText.getCurrentHintTextColor());
        assertEquals(
                ColorStateList.valueOf(SemanticColorUtils.getColorSurfaceContainerHigh(mActivity)),
                backgroundDrawable.getColor());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
    public void testHubSearchEnabledState() {
        mPropertyModel.set(HUB_SEARCH_ENABLED_STATE, false);
        assertFalse(mSearchBox.isEnabled());
        assertFalse(mSearchBoxText.isEnabled());
        assertFalse(mSearchLoupe.isEnabled());

        mPropertyModel.set(HUB_SEARCH_ENABLED_STATE, true);
        assertTrue(mSearchBox.isEnabled());
        assertTrue(mSearchBoxText.isEnabled());
        assertTrue(mSearchLoupe.isEnabled());
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
    public void testHubColorMixer_searchBoxEnabled() {
        verify(mColorMixer, times(8)).registerBlend(any());
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.ANDROID_HUB_SEARCH)
    public void testHubColorMixer_searchBoxDisabled() {
        verify(mColorMixer, times(5)).registerBlend(any());
    }

    /**
     * Setting the color twice forces {@link HubColorMixerImpl} to make a color scheme change
     * without an animation.
     */
    private void forceSetColorScheme(@HubColorScheme int colorScheme) {
        for (int i = 0; i < 2; i++) {
            mPane = mock();
            when(mPane.getColorScheme()).thenReturn(colorScheme);
            mFocusedPaneSupplier.set(mPane);
        }
    }
}
