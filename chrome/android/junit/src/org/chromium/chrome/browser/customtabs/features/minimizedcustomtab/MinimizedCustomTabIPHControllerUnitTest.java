// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.view.ViewStub;

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
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link MinimizedCustomTabIPHController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_NATIVE_INITIALIZATION
})
public class MinimizedCustomTabIPHControllerUnitTest {
    @Rule
    public ActivityScenarioRule<CustomTabActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(CustomTabActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityTabProvider mTabProvider;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Supplier<Profile> mProfileSupplier;
    @Mock private Tracker mTracker;
    @Mock private Tab mTab;

    private Activity mActivity;
    private MinimizedCustomTabIPHController mController;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.CCT_MINIMIZED_FEATURE)).thenReturn(true);
        TrackerFactory.setTrackerForTests(mTracker);
        when(mProfileSupplier.hasValue()).thenReturn(true);
        mController =
                new MinimizedCustomTabIPHController(
                        mActivity, mTabProvider, mUserEducationHelper, mProfileSupplier);
        ViewStub minimizeStub = mActivity.findViewById(R.id.minimize_button_stub);
        View minimizeView = minimizeStub.inflate();
        minimizeView.setVisibility(View.VISIBLE);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    public void testShowsIPHOnPageLoad() {
        var tabObserver = mController.getTabObserverForTesting();
        tabObserver.onPageLoadFinished(mTab, JUnitTestGURLs.EXAMPLE_URL);
        var captor = ArgumentCaptor.forClass(IPHCommand.class);
        verify(mUserEducationHelper).requestShowIPH(captor.capture());
        var cmd = captor.getValue();
        assertEquals(FeatureConstants.CCT_MINIMIZED_FEATURE, cmd.featureName);
        assertEquals(R.string.custom_tab_minimize_button_iph_bubble_text, cmd.stringId);
        assertEquals(
                R.string.custom_tab_minimize_button_iph_bubble_text, cmd.accessibilityStringId);
        assertEquals(mActivity.findViewById(R.id.custom_tabs_minimize_button), cmd.anchorView);
        assertEquals(HighlightShape.CIRCLE, cmd.highlightParams.getShape());
    }

    @Test
    public void testNotifyUserEngaged() {
        var captor = ArgumentCaptor.forClass(Callback.class);
        mController.notifyUserEngaged();
        verify(mTracker).addOnInitializedCallback(captor.capture());
        captor.getValue().onResult(true);
        verify(mTracker).notifyEvent(eq(EventConstants.CCT_MINIMIZE_BUTTON_CLICKED));
    }
}
