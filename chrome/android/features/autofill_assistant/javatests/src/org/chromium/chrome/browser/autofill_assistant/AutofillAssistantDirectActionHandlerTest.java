// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.os.Bundle;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.directactions.DirectActionHandler;
import org.chromium.chrome.browser.directactions.DirectActionReporter;
import org.chromium.chrome.browser.directactions.DirectActionReporter.Type;
import org.chromium.chrome.browser.directactions.FakeDirectActionReporter;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Tests the direct actions exposed by AA. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class AutofillAssistantDirectActionHandlerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ChromeActivity mActivity;
    private BottomSheetController mBottomSheetController;
    private DirectActionHandler mHandler;
    private TestingAutofillAssistantModuleEntryProvider mModuleEntryProvider;
    private final SharedPreferencesManager mSharedPreferencesManager =
            SharedPreferencesManager.getInstance();

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();

        mBottomSheetController = TestThreadUtils.runOnUiThreadBlocking(
                () -> AutofillAssistantUiTestUtil.getBottomSheetController(mActivity));
        mModuleEntryProvider = new TestingAutofillAssistantModuleEntryProvider();
        mModuleEntryProvider.setCannotInstall();

        mHandler = new AutofillAssistantDirectActionHandler(mActivity, mBottomSheetController,
                mActivity.getBrowserControlsManager(), mActivity.getCompositorViewHolder(),
                mActivity.getActivityTabProvider(), mModuleEntryProvider);

        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED);
        mSharedPreferencesManager.removeKey(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN);
    }

    @Test
    @MediumTest
    public void testReportOnboardingOnlyIfNotAccepted() throws Exception {
        mModuleEntryProvider.setInstalled();

        FakeDirectActionReporter reporter = new FakeDirectActionReporter();
        reportAvailableDirectActions(mHandler, reporter);

        assertEquals(1, reporter.mActions.size());

        FakeDirectActionReporter.FakeDefinition onboarding = reporter.mActions.get(0);
        assertEquals("onboarding", onboarding.mId);
        assertEquals(2, onboarding.mParameters.size());
        assertEquals("name", onboarding.mParameters.get(0).mName);
        assertEquals(Type.STRING, onboarding.mParameters.get(0).mType);
        assertEquals("experiment_ids", onboarding.mParameters.get(1).mName);
        assertEquals(Type.STRING, onboarding.mParameters.get(1).mType);
        assertEquals(1, onboarding.mResults.size());
        assertEquals("success", onboarding.mResults.get(0).mName);
        assertEquals(Type.BOOLEAN, onboarding.mResults.get(0).mType);
    }

    @Test
    @MediumTest
    public void testReportAvailableDirectActions() throws Exception {
        mModuleEntryProvider.setInstalled();
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        // Start the autofill assistant stack.

        FakeDirectActionReporter reporter = new FakeDirectActionReporter();
        reportAvailableDirectActions(mHandler, reporter);

        assertEquals(1, reporter.mActions.size());

        FakeDirectActionReporter.FakeDefinition fetch = reporter.mActions.get(0);
        assertEquals("fetch_website_actions", fetch.mId);
        assertEquals(2, fetch.mParameters.size());
        assertEquals("user_name", fetch.mParameters.get(0).mName);
        assertEquals(Type.STRING, fetch.mParameters.get(0).mType);
        assertEquals(false, fetch.mParameters.get(0).mRequired);
        assertEquals("experiment_ids", fetch.mParameters.get(1).mName);
        assertEquals(Type.STRING, fetch.mParameters.get(1).mType);
        assertEquals(false, fetch.mParameters.get(1).mRequired);
        assertEquals(1, fetch.mResults.size());
        assertEquals("success", fetch.mResults.get(0).mName);
        assertEquals(Type.BOOLEAN, fetch.mResults.get(0).mType);

        // Start the autofill assistant stack.
        fetchWebsiteActions();
        // Reset the reported actions.
        reporter = new FakeDirectActionReporter();
        reportAvailableDirectActions(mHandler, reporter);

        // Now that the AA stack is up, the fetdch_website_actions should no longer show up.
        assertEquals(3, reporter.mActions.size());

        // Now we expect 3 dyamic actions "search", "action2" and "action2_alias".
        FakeDirectActionReporter.FakeDefinition search = reporter.mActions.get(0);
        assertEquals("search", search.mId);
        assertEquals(3, search.mParameters.size());
        assertEquals("experiment_ids", search.mParameters.get(0).mName);
        assertEquals(Type.STRING, search.mParameters.get(0).mType);
        assertEquals("SEARCH_QUERY", search.mParameters.get(1).mName);
        assertEquals(Type.STRING, search.mParameters.get(1).mType);
        assertEquals("arg2", search.mParameters.get(2).mName);
        assertEquals(Type.STRING, search.mParameters.get(2).mType);
        assertEquals(1, search.mResults.size());
        assertEquals("success", search.mResults.get(0).mName);
        assertEquals(Type.BOOLEAN, search.mResults.get(0).mType);

        FakeDirectActionReporter.FakeDefinition action2 = reporter.mActions.get(1);
        assertEquals("action2", action2.mId);
        assertEquals(3, action2.mParameters.size());
        assertEquals("experiment_ids", action2.mParameters.get(0).mName);
        assertEquals(Type.STRING, action2.mParameters.get(0).mType);
        assertEquals("SEARCH_QUERY", action2.mParameters.get(1).mName);
        assertEquals(Type.STRING, action2.mParameters.get(1).mType);
        assertEquals("arg2", action2.mParameters.get(2).mName);
        assertEquals(Type.STRING, action2.mParameters.get(2).mType);
        assertEquals(1, action2.mResults.size());
        assertEquals("success", action2.mResults.get(0).mName);
        assertEquals(Type.BOOLEAN, action2.mResults.get(0).mType);

        FakeDirectActionReporter.FakeDefinition action2Alias = reporter.mActions.get(2);
        assertEquals("action2_alias", action2Alias.mId);
        assertEquals(3, action2Alias.mParameters.size());
        assertEquals("experiment_ids", action2Alias.mParameters.get(0).mName);
        assertEquals(Type.STRING, action2Alias.mParameters.get(0).mType);
        assertEquals("SEARCH_QUERY", action2Alias.mParameters.get(1).mName);
        assertEquals(Type.STRING, action2Alias.mParameters.get(1).mType);
        assertEquals("arg2", action2Alias.mParameters.get(2).mName);
        assertEquals(Type.STRING, action2Alias.mParameters.get(2).mType);
        assertEquals(1, action2Alias.mResults.size());
        assertEquals("success", action2Alias.mResults.get(0).mName);
        assertEquals(Type.BOOLEAN, action2Alias.mResults.get(0).mType);
    }

    @Test
    @MediumTest
    public void testReportAvailableAutofillAssistantActions() throws Exception {
        mModuleEntryProvider.setInstalled();
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);

        FakeDirectActionReporter reporter = new FakeDirectActionReporter();
        reportAvailableDirectActions(mHandler, reporter);

        assertEquals(1, reporter.mActions.size());

        FakeDirectActionReporter.FakeDefinition fetch = reporter.mActions.get(0);
        assertEquals("fetch_website_actions", fetch.mId);
        assertEquals(2, fetch.mParameters.size());
        assertEquals("user_name", fetch.mParameters.get(0).mName);
        assertEquals(Type.STRING, fetch.mParameters.get(0).mType);
        assertEquals(false, fetch.mParameters.get(0).mRequired);
        assertEquals("experiment_ids", fetch.mParameters.get(1).mName);
        assertEquals(Type.STRING, fetch.mParameters.get(1).mType);
        assertEquals(false, fetch.mParameters.get(1).mRequired);

        assertEquals(1, fetch.mResults.size());
        assertEquals("success", fetch.mResults.get(0).mName);
        assertEquals(Type.BOOLEAN, fetch.mResults.get(0).mType);
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = 22) // TODO(crbug/990118): re-enable
    public void testOnboarding() throws Exception {
        mModuleEntryProvider.setInstalled();

        assertThat(isOnboardingReported(), is(true));
        acceptOnboarding();

        assertTrue(AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted());
    }

    @Test
    @MediumTest
    public void testModuleNotAvailable() throws Exception {
        mModuleEntryProvider.setCannotInstall();

        assertThat(isOnboardingReported(), is(true));
        assertFalse(performAction("onboarding", Bundle.EMPTY));
    }

    @Test
    @MediumTest
    @DisableIf.Build(sdk_is_greater_than = 22) // TODO(crbug/990118): re-enable
    public void testInstallModuleOnDemand() throws Exception {
        mModuleEntryProvider.setNotInstalled();

        assertThat(isOnboardingReported(), is(true));
        acceptOnboarding();
    }

    private void acceptOnboarding() throws Exception {
        WaitingCallback<Boolean> onboardingCallback =
                performActionAsync("onboarding", Bundle.EMPTY);

        waitUntilViewMatchesCondition(withId(R.id.button_init_ok), isCompletelyDisplayed());

        assertFalse(onboardingCallback.hasResult());
        onView(withId(R.id.button_init_ok)).perform(click());
        assertEquals(Boolean.TRUE, onboardingCallback.waitForResult("accept onboarding"));
    }

    private boolean isOnboardingReported() throws Exception {
        FakeDirectActionReporter reporter = new FakeDirectActionReporter();
        reportAvailableDirectActions(mHandler, reporter);

        for (FakeDirectActionReporter.FakeDefinition definition : reporter.mActions) {
            if (definition.mId.equals("onboarding")) {
                return true;
            }
        }
        return false;
    }

    // TODO(b/134741524): Add tests that list and execute direct actions coming from scripts, once
    // we have a way to fake RPCs and can create a bottom sheet controller on demand.

    /** Calls fetch_website_actions and returns whether that succeeded or not. */
    private boolean fetchWebsiteActions() throws Exception {
        WaitingCallback<Bundle> callback = new WaitingCallback<Bundle>();
        assertTrue(TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mHandler.performDirectAction(
                                "fetch_website_actions", Bundle.EMPTY, callback)));
        return callback.waitForResult("fetch_website_actions").getBoolean("success", false);
    }

    /**
     * When reporting direct actions involves web_contents in the controller, it needs to run on the
     * UI thread.
     */
    private void reportAvailableDirectActions(
            DirectActionHandler handler, DirectActionReporter reporter) throws Exception {
        assertTrue(TestThreadUtils.runOnUiThreadBlocking(() -> {
            handler.reportAvailableDirectActions(reporter);
            return true;
        }));
    }

    /** Performs direct action |name| and returns the result. */
    private Boolean performAction(String name, Bundle arguments) throws Exception {
        return performActionAsync(name, arguments).waitForResult("success");
    }

    /**
     * Performs direct action |name| and returns a {@link WaitingCallback} that'll eventually
     * contain the result.
     */
    private WaitingCallback<Boolean> performActionAsync(String name, Bundle arguments)
            throws Exception {
        WaitingCallback<Boolean> callback = new WaitingCallback<Boolean>();
        Bundle allArguments = new Bundle(arguments);
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mHandler.performDirectAction(name, allArguments,
                                (bundle) -> callback.onResult(bundle.getBoolean("success"))));
        return callback;
    }

    /**
     * A callback that allows waiting for the result to be available, then retrieving it.
     */
    private static class WaitingCallback<T> implements Callback<T> {
        private final CallbackHelper mHelper = new CallbackHelper();
        private boolean mHasResult;
        private T mResult;

        @Override
        public synchronized void onResult(T result) {
            mResult = result;
            mHasResult = true;
            mHelper.notifyCalled();
        }

        synchronized T waitForResult(String msg) throws Exception {
            if (!mHasResult) mHelper.waitForFirst(msg);
            assertTrue(mHasResult);
            return mResult;
        }

        synchronized boolean hasResult() {
            return mHasResult;
        }

        synchronized T getResult() {
            return mResult;
        }
    }
}
