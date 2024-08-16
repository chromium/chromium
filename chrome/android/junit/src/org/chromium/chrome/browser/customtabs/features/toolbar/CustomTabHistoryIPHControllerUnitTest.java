// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link CustomTabHistoryIPHController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.APP_SPECIFIC_HISTORY)
public class CustomTabHistoryIPHControllerUnitTest {
    @Rule
    public ActivityScenarioRule<CustomTabActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(CustomTabActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityTabProvider mTabProvider;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Profile mMockProfile;
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private Tracker mTracker;
    @Mock private Tab mTab;

    @Mock private Activity mActivity;
    private CustomTabHistoryIPHController mController;

    @Before
    public void setUp() {
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.CCT_HISTORY_FEATURE)).thenReturn(true);
        TrackerFactory.setTrackerForTests(mTracker);
        when(mMockProfile.isOffTheRecord()).thenReturn(false);
        var profileSupplier = new ObservableSupplierImpl<>(mMockProfile);
        mController =
                new CustomTabHistoryIPHController(
                        mActivity, mTabProvider, profileSupplier, mAppMenuHandler);
        mController.setUserEducationHelperForTesting(mUserEducationHelper);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    public void testShowsIPHOnPageLoad() {
        var tabObserver = mController.getTabObserverForTesting();
        tabObserver.onPageLoadStarted(mTab, JUnitTestGURLs.EXAMPLE_URL);
        var captor = ArgumentCaptor.forClass(IPHCommand.class);
        verify(mUserEducationHelper).requestShowIPH(captor.capture());
        var cmd = captor.getValue();
        assertEquals(FeatureConstants.CCT_HISTORY_FEATURE, cmd.featureName);
        assertEquals(R.string.custom_tab_history_iph_bubble_text, cmd.stringId);
        assertEquals(R.string.custom_tab_history_iph_bubble_text, cmd.accessibilityStringId);
        assertEquals(mActivity.findViewById(R.id.menu_button_wrapper), cmd.anchorView);
        assertEquals(HighlightShape.CIRCLE, cmd.highlightParams.getShape());
    }

    @Test
    public void testNoIPHInOTR() {
        when(mMockProfile.isOffTheRecord()).thenReturn(true);
        var tabObserver = mController.getTabObserverForTesting();
        tabObserver.onPageLoadStarted(mTab, JUnitTestGURLs.EXAMPLE_URL);
        verifyNoInteractions(mUserEducationHelper);
    }

    @Test
    public void testNotifyUserEngaged() {
        var captor = ArgumentCaptor.forClass(Callback.class);
        mController.notifyUserEngaged();
        verify(mTracker).addOnInitializedCallback(captor.capture());
        captor.getValue().onResult(true);
        verify(mTracker).notifyEvent(eq(EventConstants.CCT_HISTORY_MENU_ITEM_CLICKED));
    }

    @Test
    public void testMenuHighlight() {
        mController.setHighlightMenuItemForTesting(true);
        verify(mAppMenuHandler).setMenuHighlight(R.id.open_history_menu_id);
        mController.setHighlightMenuItemForTesting(false);
        verify(mAppMenuHandler).clearMenuHighlight();
    }
}
