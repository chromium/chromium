// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.content.Context;
import android.content.Intent;
import android.util.Pair;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.RemoteViews;
import android.widget.TextView;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.firstrun.FirstRunActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityPreferencesManager.SearchActivityPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for the SearchWidgetProvider. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class SearchWidgetProviderTest {
    private static final class TestDelegate
            extends SearchWidgetProvider.SearchWidgetProviderDelegate {
        public static final int[] ALL_IDS = {11684, 20170525};

        public final List<Pair<Integer, RemoteViews>> mViews = new ArrayList<>();
        private Context mContext;

        private TestDelegate(Context context) {
            super(context);
            mContext = context;
        }

        @Override
        protected Context getContext() {
            return mContext;
        }

        @Override
        protected int[] getAllSearchWidgetIds() {
            return ALL_IDS;
        }

        @Override
        protected void updateAppWidget(int id, RemoteViews views) {
            mViews.add(new Pair<Integer, RemoteViews>(id, views));
        }
    }

    private static final class TestContext extends AdvancedMockContext {
        public TestContext() {
            super(
                    InstrumentationRegistry.getInstrumentation()
                            .getTargetContext()
                            .getApplicationContext());
        }
    }

    private static final String TEXT_GENERIC = "Search";
    private static final String TEXT_SEARCH_ENGINE = "Stuff and Thangs";
    private static final String TEXT_SEARCH_ENGINE_FULL = "Search with Stuff and Thangs";

    private TestContext mContext;
    private TestDelegate mDelegate;

    @Before
    public void setUp() {
        mContext = new TestContext();
        mDelegate = new TestDelegate(mContext);
        SearchWidgetProvider.setActivityDelegateForTest(mDelegate);
    }

    /**
     * Update the SearchWidgetProvider with the supplied information. Guarantees that the update
     * will be performed on the UI thread.
     *
     * @param searchEngineName The new search engine name.
     * @param voiceSearchAvailable Whether voice search is available.
     */
    private void performUpdate(String searchEngineName, boolean voiceSearchAvailable) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SearchWidgetProvider.performUpdate(
                            null,
                            new SearchActivityPreferences(
                                    searchEngineName,
                                    null,
                                    voiceSearchAvailable,
                                    /* lensAvailable= */ false,
                                    /* incognitoAvailable= */ true));
                });
    }

    /**
     * Instrument the LocaleManager to report that the FirstRunExperience has not yet been run and
     * that the default search engine is not yet selected.
     */
    private void setNeedToCheckForSearchEnginePromo() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocaleManager.getInstance()
                            .setDelegateForTest(
                                    new LocaleManagerDelegate() {
                                        @Override
                                        public boolean needToCheckForSearchEnginePromo() {
                                            return false;
                                        }
                                    });
                });
    }

    @Test
    @SmallTest
    public void testUpdateAll() {
        // Without any idea of what the default search engine is, widgets should default to saying
        // just "Search".
        performUpdate(null, true);
        checkWidgetStates(TEXT_GENERIC, View.VISIBLE);

        // The microphone icon should disappear if voice queries are unavailable.
        mDelegate.mViews.clear();
        performUpdate(null, false);
        checkWidgetStates(TEXT_GENERIC, View.GONE);

        // Text should reappear when the default search engine is known.
        mDelegate.mViews.clear();
        performUpdate(TEXT_SEARCH_ENGINE, false);
        checkWidgetStates(TEXT_GENERIC, View.GONE);

        // After recording that the default search engine is "X" and search engine promo check,
        // it should say "Search with X".
        mDelegate.mViews.clear();
        setNeedToCheckForSearchEnginePromo();
        performUpdate(TEXT_SEARCH_ENGINE, false);
        checkWidgetStates(TEXT_SEARCH_ENGINE_FULL, View.GONE);

        // The microphone icon should appear if voice queries are available.
        mDelegate.mViews.clear();
        performUpdate(TEXT_SEARCH_ENGINE, true);
        checkWidgetStates(TEXT_SEARCH_ENGINE_FULL, View.VISIBLE);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testUpdateCachedEngineNameBeforeFirstRun() throws ExecutionException {
        Assert.assertFalse(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> SearchWidgetProvider.shouldShowFullString()));

        // Without any idea of what the default search engine is, widgets should default to saying
        // just "Search".
        performUpdate(null, true);
        checkWidgetStates(TEXT_GENERIC, View.VISIBLE);

        // Until First Run is complete, no search engine branding should be displayed.  Widgets are
        // already displaying the generic string, and should continue doing so, so they don't get
        // updated.
        mDelegate.mViews.clear();
        setNeedToCheckForSearchEnginePromo();
        Assert.assertEquals(0, mDelegate.mViews.size());

        // Manually set the preference, then update the cached engine name again.  The
        // SearchWidgetProvider should now believe that its widgets are displaying branding when it
        // isn't allowed to, then update them.
        mDelegate.mViews.clear();
        mDelegate
                .getChromeSharedPreferences()
                .writeString(
                        ChromePreferenceKeys.SEARCH_WIDGET_SEARCH_ENGINE_SHORTNAME,
                        TEXT_SEARCH_ENGINE);
        performUpdate(TEXT_SEARCH_ENGINE, true);
        checkWidgetStates(TEXT_GENERIC, View.VISIBLE);
    }

    private void checkWidgetStates(final String expectedString, final int expectedMicrophoneState) {
        // Confirm that all the widgets got updated.
        Assert.assertEquals(TestDelegate.ALL_IDS.length, mDelegate.mViews.size());
        for (int i = 0; i < TestDelegate.ALL_IDS.length; i++) {
            Assert.assertEquals(TestDelegate.ALL_IDS[i], mDelegate.mViews.get(i).first.intValue());
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Check the contents of the RemoteViews by inflating them.
                    for (int i = 0; i < mDelegate.mViews.size(); i++) {
                        FrameLayout parentView = new FrameLayout(mContext);
                        RemoteViews views = mDelegate.mViews.get(i).second;
                        View view = views.apply(mContext, parentView);
                        parentView.addView(view);

                        // Confirm that the string is correct.
                        TextView titleView = view.findViewById(R.id.title);
                        Assert.assertEquals(View.VISIBLE, titleView.getVisibility());
                        Assert.assertEquals(expectedString, titleView.getHint());

                        // Confirm the visibility of the microphone.
                        View microphoneView = view.findViewById(R.id.microphone_icon);
                        Assert.assertEquals(
                                expectedMicrophoneState, microphoneView.getVisibility());
                    }
                });
    }

    @Test
    @SmallTest
    public void testMicrophoneClick() {
        for (int i = 0; i < mDelegate.mViews.size(); i++) {
            RemoteViews views = mDelegate.mViews.get(i).second;
            clickOnWidget(views, R.id.microphone_icon, true);
        }
    }

    @Test
    @SmallTest
    public void testTextClick() {
        for (int i = 0; i < mDelegate.mViews.size(); i++) {
            RemoteViews views = mDelegate.mViews.get(i).second;
            clickOnWidget(views, R.id.text_container, true);
        }
    }

    @Test
    @SmallTest
    @CommandLineFlags.Remove(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
    public void testOnboardingRequired() {
        for (int i = 0; i < mDelegate.mViews.size(); i++) {
            RemoteViews views = mDelegate.mViews.get(i).second;
            clickOnWidget(views, R.id.text_container, false);
        }
    }

    private void clickOnWidget(
            final RemoteViews views, final int clickTarget, boolean isFirstRunComplete) {
        String className =
                isFirstRunComplete
                        ? SearchActivity.class.getName()
                        : FirstRunActivity.class.getName();
        ActivityMonitor monitor = new ActivityMonitor(className, null, false);

        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        instrumentation.addMonitor(monitor);

        // Click on the widget.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FrameLayout parentView = new FrameLayout(mContext);
                    View view = views.apply(mContext, parentView);
                    parentView.addView(view);
                    view.findViewById(clickTarget).performClick();
                });

        Activity activity =
                instrumentation.waitForMonitorWithTimeout(
                        monitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull(activity);
        if (isFirstRunComplete) {
            // Check that the Activity was launched in the right mode.
            Intent intent = activity.getIntent();
            boolean microphoneState =
                    SearchActivityUtils.getIntentSearchType(intent) == SearchType.VOICE;
            Assert.assertEquals(clickTarget == R.id.microphone_icon, microphoneState);
            boolean fromWidget =
                    IntentUtils.safeGetBooleanExtra(
                            intent, SearchWidgetProvider.EXTRA_FROM_SEARCH_WIDGET, false);
            Assert.assertTrue(fromWidget);
        }
    }

    @Test
    @SmallTest
    public void testCrashAbsorption() {
        Runnable crashingRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        throw new RuntimeException();
                    }
                };

        SharedPreferencesManager prefs = mDelegate.getChromeSharedPreferences();
        Assert.assertEquals(0, SearchWidgetProvider.getNumConsecutiveCrashes(prefs));

        // The first few crashes should be silently absorbed.
        SearchWidgetProvider.run(crashingRunnable);
        Assert.assertEquals(1, SearchWidgetProvider.getNumConsecutiveCrashes(prefs));
        SearchWidgetProvider.run(crashingRunnable);
        Assert.assertEquals(2, SearchWidgetProvider.getNumConsecutiveCrashes(prefs));

        // The crash should be thrown after hitting the crash limit, which is 3.
        boolean exceptionWasThrown = false;
        try {
            SearchWidgetProvider.run(crashingRunnable);
        } catch (Exception e) {
            exceptionWasThrown = true;
        }
        Assert.assertEquals(3, SearchWidgetProvider.getNumConsecutiveCrashes(prefs));
        Assert.assertTrue(exceptionWasThrown);
    }
}
