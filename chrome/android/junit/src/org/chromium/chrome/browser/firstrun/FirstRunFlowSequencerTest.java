// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.accounts.Account;
import android.app.Activity;
import android.os.Bundle;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.ChildAccountStatus;

import java.util.Collections;
import java.util.List;

/**
 * Tests FirstRunFlowSequencer which contains the core logic of what should be shown during the
 * first run.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMultiDex.class})
public class FirstRunFlowSequencerTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    /** Information for Google OS account */
    private static final String GOOGLE_ACCOUNT_TYPE = "com.google";
    private static final String DEFAULT_ACCOUNT = "test@gmail.com";

    /**
     * Testing version of FirstRunFlowSequencer that allows us to override all needed checks.
     */
    public static class TestFirstRunFlowSequencer extends FirstRunFlowSequencer {
        public Bundle returnedBundle;
        public boolean calledOnFlowIsKnown;
        public boolean calledSetFirstRunFlowSignInComplete;

        public boolean isFirstRunFlowComplete;
        public boolean isSignedIn;
        public boolean isSyncAllowed;
        public List<Account> googleAccounts;
        public boolean shouldSkipFirstUseHints;
        public boolean isFirstRunEulaAccepted;
        public boolean shouldShowDataReductionPage;
        public boolean shouldShowSearchEnginePage;

        public TestFirstRunFlowSequencer(Activity activity) {
            super(activity);
        }

        @Override
        public void onFlowIsKnown(Bundle freProperties) {
            calledOnFlowIsKnown = true;
            if (freProperties != null) onNativeAndPoliciesInitialized(freProperties);
            returnedBundle = freProperties;
        }

        @Override
        public boolean isFirstRunFlowComplete() {
            return isFirstRunFlowComplete;
        }

        @Override
        public boolean isSignedIn() {
            return isSignedIn;
        }

        @Override
        public boolean isSyncAllowed() {
            return isSyncAllowed;
        }

        @Override
        public List<Account> getGoogleAccounts() {
            return googleAccounts;
        }

        @Override
        public boolean shouldSkipFirstUseHints() {
            return shouldSkipFirstUseHints;
        }

        @Override
        public boolean isFirstRunEulaAccepted() {
            return isFirstRunEulaAccepted;
        }

        @Override
        public boolean shouldShowDataReductionPage() {
            return shouldShowDataReductionPage;
        }

        @Override
        public boolean shouldShowSearchEnginePage() {
            return shouldShowSearchEnginePage;
        }

        @Override
        protected void setFirstRunFlowSignInComplete() {
            calledSetFirstRunFlowSignInComplete = true;
        }
    }

    private ActivityController<Activity> mActivityController;
    private TestFirstRunFlowSequencer mSequencer;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(Activity.class);
        mSequencer = new TestFirstRunFlowSequencer(mActivityController.setup().get());
    }

    @After
    public void tearDown() {
        mActivityController.pause().stop().destroy();
    }

    @Test
    @Feature({"FirstRun"})
    public void testFirstRunComplete() {
        mSequencer.isFirstRunFlowComplete = true;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts =
                Collections.singletonList(new Account(DEFAULT_ACCOUNT, GOOGLE_ACCOUNT_TYPE));
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.isFirstRunEulaAccepted = true;
        mSequencer.initializeSharedState(ChildAccountStatus.NOT_CHILD);

        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertNull(mSequencer.returnedBundle);
    }

    @Test
    @Feature({"FirstRun"})
    public void testStandardFlowTosNotSeen() {
        mSequencer.isFirstRunFlowComplete = false;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts = Collections.emptyList();
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.shouldShowDataReductionPage = false;
        mSequencer.initializeSharedState(ChildAccountStatus.NOT_CHILD);

        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertFalse(mSequencer.calledSetFirstRunFlowSignInComplete);

        Bundle bundle = mSequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SIGNIN_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_DATA_REDUCTION_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(ChildAccountStatus.NOT_CHILD,
                bundle.getInt(SyncConsentFirstRunFragment.CHILD_ACCOUNT_STATUS));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testStandardFlowOneChildAccount() {
        mSequencer.isFirstRunFlowComplete = false;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts =
                Collections.singletonList(new Account(DEFAULT_ACCOUNT, GOOGLE_ACCOUNT_TYPE));
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.shouldShowDataReductionPage = false;
        mSequencer.initializeSharedState(ChildAccountStatus.REGULAR_CHILD);

        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertTrue(mSequencer.calledSetFirstRunFlowSignInComplete);

        Bundle bundle = mSequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SIGNIN_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_DATA_REDUCTION_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(ChildAccountStatus.REGULAR_CHILD,
                bundle.getInt(SyncConsentFirstRunFragment.CHILD_ACCOUNT_STATUS));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testStandardFlowShowDataReductionPage() {
        mSequencer.isFirstRunFlowComplete = false;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts = Collections.emptyList();
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.shouldShowDataReductionPage = true;
        mSequencer.shouldShowSearchEnginePage = false;
        mSequencer.initializeSharedState(ChildAccountStatus.NOT_CHILD);

        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertFalse(mSequencer.calledSetFirstRunFlowSignInComplete);

        Bundle bundle = mSequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SIGNIN_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_DATA_REDUCTION_PAGE));
        assertFalse(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(ChildAccountStatus.NOT_CHILD,
                bundle.getInt(SyncConsentFirstRunFragment.CHILD_ACCOUNT_STATUS));
        assertEquals(4, bundle.size());
    }

    @Test
    @Feature({"FirstRun"})
    public void testStandardFlowShowSearchEnginePage() {
        mSequencer.isFirstRunFlowComplete = false;
        mSequencer.isSignedIn = false;
        mSequencer.isSyncAllowed = true;
        mSequencer.googleAccounts = Collections.emptyList();
        mSequencer.shouldSkipFirstUseHints = false;
        mSequencer.shouldShowDataReductionPage = true;
        mSequencer.shouldShowSearchEnginePage = true;
        mSequencer.initializeSharedState(ChildAccountStatus.NOT_CHILD);

        mSequencer.processFreEnvironmentPreNative();
        assertTrue(mSequencer.calledOnFlowIsKnown);
        assertFalse(mSequencer.calledSetFirstRunFlowSignInComplete);

        Bundle bundle = mSequencer.returnedBundle;
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SIGNIN_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_DATA_REDUCTION_PAGE));
        assertTrue(bundle.getBoolean(FirstRunActivityBase.SHOW_SEARCH_ENGINE_PAGE));
        assertEquals(ChildAccountStatus.NOT_CHILD,
                bundle.getInt(SyncConsentFirstRunFragment.CHILD_ACCOUNT_STATUS));
        assertEquals(4, bundle.size());
    }
}
