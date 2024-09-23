// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/**
 * Tests that when DeferredStartupHandler#queueDeferredTasksOnIdleHandler() is run that the
 * activity's tab has finished loading.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabDeferredStartupTest {
    static class PageLoadFinishedTabObserver extends EmptyTabObserver {
        private boolean mIsPageLoadFinished;

        @Override
        public void onPageLoadFinished(Tab tab, GURL url) {
            mIsPageLoadFinished = true;
        }

        public boolean isPageLoadFinished() {
            return mIsPageLoadFinished;
        }
    }

    static class InitialTabCreationObserver extends CustomTabActivityTabProvider.Observer {
        private TabObserver mObserver;

        public InitialTabCreationObserver(TabObserver observer) {
            mObserver = observer;
        }

        @Override
        public void onInitialTabCreated(@NonNull Tab tab, @TabCreationMode int mode) {
            tab.addObserver(mObserver);
        }
    }

    static class NewTabObserver
            implements TabModelSelectorObserver,
                    ApplicationStatus.ActivityStateListener,
                    InflationObserver {
        private BaseCustomTabActivity mActivity;
        private TabObserver mObserver;

        public NewTabObserver(TabObserver observer) {
            mObserver = observer;
        }

        @Override
        public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
            tab.addObserver(mObserver);
        }

        @Override
        public void onActivityStateChange(Activity activity, @ActivityState int newState) {
            if (newState == ActivityState.CREATED
                    && activity instanceof BaseCustomTabActivity
                    && mActivity == null) {
                mActivity = (BaseCustomTabActivity) activity;
                mActivity.getLifecycleDispatcher().register(this);
            }
        }

        @Override
        public void onPreInflationStartup() {
            BaseCustomTabActivityComponent baseCustomTabActivityComponent =
                    (BaseCustomTabActivityComponent) mActivity.getComponent();
            baseCustomTabActivityComponent
                    .resolveTabProvider()
                    .addObserver(new InitialTabCreationObserver(mObserver));
        }

        @Override
        public void onPostInflationStartup() {}
    }

    static class PageIsLoadedDeferredStartupHandler extends DeferredStartupHandler {
        public PageIsLoadedDeferredStartupHandler(
                PageLoadFinishedTabObserver observer, CallbackHelper helper) {
            mObserver = observer;
            mHelper = helper;
        }

        @Override
        public void queueDeferredTasksOnIdleHandler() {
            Assert.assertTrue("Page is yet to finish loading.", mObserver.isPageLoadFinished());

            mHelper.notifyCalled();

            super.queueDeferredTasksOnIdleHandler();
        }

        private CallbackHelper mHelper;
        private PageLoadFinishedTabObserver mObserver;
    }

    @ClassParameter
    public static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(ActivityType.WEBAPP).name("Webapp"),
                    new ParameterSet().value(ActivityType.CUSTOM_TAB).name("CustomTab"),
                    new ParameterSet()
                            .value(ActivityType.TRUSTED_WEB_ACTIVITY)
                            .name("TrustedWebActivity"));

    private @ActivityType int mActivityType;

    @Rule public final ChromeActivityTestRule<?> mActivityTestRule;

    public CustomTabDeferredStartupTest(@ActivityType int activityType) {
        mActivityType = activityType;
        mActivityTestRule = CustomTabActivityTypeTestUtils.createActivityTestRule(activityType);
    }

    @Test
    @LargeTest
    // TODO(eirage): Make this test work with quality enforcement.
    public void testPageIsLoadedOnDeferredStartup() throws Exception {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PageLoadFinishedTabObserver tabObserver = new PageLoadFinishedTabObserver();
                    NewTabObserver newTabObserver = new NewTabObserver(tabObserver);
                    TabModelSelectorBase.setObserverForTests(newTabObserver);
                    ApplicationStatus.registerStateListenerForAllActivities(newTabObserver);
                    PageIsLoadedDeferredStartupHandler handler =
                            new PageIsLoadedDeferredStartupHandler(tabObserver, helper);
                    DeferredStartupHandler.setInstanceForTests(handler);
                });
        CustomTabActivityTypeTestUtils.launchActivity(
                mActivityType, mActivityTestRule, "about:blank");
        helper.waitForCallback(0);
    }
}
