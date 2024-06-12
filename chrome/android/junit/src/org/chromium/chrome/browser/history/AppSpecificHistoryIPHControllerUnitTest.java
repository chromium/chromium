// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.user_education.IPHCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

/** Unit tests for {@link AppSpecificHistoryIPHController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.APP_SPECIFIC_HISTORY)
public class AppSpecificHistoryIPHControllerUnitTest {
    @Rule
    public ActivityScenarioRule<ChromeTabbedActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(ChromeTabbedActivity.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Supplier<Profile> mProfileSupplier;
    @Mock private Tracker mTracker;

    @Mock private Activity mActivity;
    private AppSpecificHistoryIPHController mController;

    @Before
    public void setUp() {
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.wouldTriggerHelpUI(FeatureConstants.APP_SPECIFIC_HISTORY_FEATURE))
                .thenReturn(true);
        TrackerFactory.setTrackerForTests(mTracker);
        when(mProfileSupplier.hasValue()).thenReturn(true);
        mController = new AppSpecificHistoryIPHController(mActivity, mProfileSupplier);
        mController.setUserEducationHelperForTesting(mUserEducationHelper);
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testShowsIPHOnPageLoad() {
        mController.maybeShowIPH();
        var captor = ArgumentCaptor.forClass(IPHCommand.class);
        verify(mUserEducationHelper).requestShowIPH(captor.capture());
        var cmd = captor.getValue();
        assertEquals(FeatureConstants.APP_SPECIFIC_HISTORY_FEATURE, cmd.featureName);
        assertEquals(R.string.history_iph_bubble_text, cmd.stringId);
        assertEquals(R.string.history_iph_bubble_text, cmd.accessibilityStringId);
        assertEquals(mActivity.findViewById(R.id.menu_button_wrapper), cmd.anchorView);
        assertEquals(HighlightShape.CIRCLE, cmd.highlightParams.getShape());
    }

    @Test
    public void testNotifyUserEngaged() {
        var captor = ArgumentCaptor.forClass(Callback.class);
        mController.notifyUserEngaged();
        verify(mTracker).addOnInitializedCallback(captor.capture());
        captor.getValue().onResult(true);
        verify(mTracker).notifyEvent(eq(EventConstants.HISTORY_TOOLBAR_SEARCH_MENU_ITEM_CLICKED));
    }
}
