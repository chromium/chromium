// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.Arrays;
import java.util.concurrent.Callable;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link WarmupManager} */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class WarmupManagerTest {
    public enum ProfileType { REGULAR_PROFILE, PRIMARY_OTR_PROFILE, NON_PRIMARY_OTR_PROFILE }

    /** Provides parameter for testPreconnect to run it with both regular and incognito profiles.*/
    public static class ProfileParams implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(new ParameterSet()
                                         .value(ProfileType.PRIMARY_OTR_PROFILE.toString())
                                         .name("PrimaryIncognitoProfile"),
                    new ParameterSet()
                            .value(ProfileType.NON_PRIMARY_OTR_PROFILE.toString())
                            .name("NonPrimaryIncognitoProfile"),
                    new ParameterSet()
                            .value(ProfileType.REGULAR_PROFILE.toString())
                            .name("RegularProfile"));
        }
    }

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    private WarmupManager mWarmupManager;
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        // Unlike most of Chrome, the WarmupManager inflates layouts with the application context.
        // This is because the inflation happens before an activity exists. If you're trying to fix
        // a failing test, it's important to not add extra theme/style information to this context
        // in this test because it could hide a real production issue. See https://crbug.com/1246329
        // for an example.
        mContext = InstrumentationRegistry.getInstrumentation()
                           .getTargetContext()
                           .getApplicationContext();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
            mWarmupManager = WarmupManager.getInstance();
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mWarmupManager.destroySpareWebContents());
    }

    private static Profile getNonPrimaryOTRProfile() {
        return TestThreadUtils.runOnUiThreadBlockingNoException((Callable<Profile>) () -> {
            OTRProfileID otrProfileID = OTRProfileID.createUnique("CCT:Incognito");
            return Profile.getLastUsedRegularProfile().getOffTheRecordProfile(
                    otrProfileID, /*createIfNeeded=*/true);
        });
    }

    private static Profile getPrimaryOTRProfile() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                (Callable<Profile>) ()
                        -> Profile.getLastUsedRegularProfile().getPrimaryOTRProfile(
                                /*createIfNeeded=*/true));
    }

    private static Profile getRegularProfile() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                (Callable<Profile>) () -> Profile.getLastUsedRegularProfile());
    }

    private static Profile getProfile(ProfileType profileType) {
        switch (profileType) {
            case NON_PRIMARY_OTR_PROFILE:
                return getNonPrimaryOTRProfile();
            case PRIMARY_OTR_PROFILE:
                return getPrimaryOTRProfile();
            default:
                return getRegularProfile();
        }
    }

    @Test
    @SmallTest
    public void testCreateAndTakeSpareRenderer() {
        final AtomicBoolean isRenderFrameLive = new AtomicBoolean();
        final AtomicReference<WebContents> webContentsReference = new AtomicReference<>();

        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            mWarmupManager.createSpareWebContents();
            Assert.assertTrue(mWarmupManager.hasSpareWebContents());
            WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
            Assert.assertNotNull(webContents);
            Assert.assertFalse(mWarmupManager.hasSpareWebContents());

            if (webContents.getMainFrame().isRenderFrameLive()) {
                isRenderFrameLive.set(true);
            }
            WebContentsObserver observer = new WebContentsObserver(webContents) {
                @Override
                public void renderFrameCreated(GlobalRenderFrameHostId id) {
                    isRenderFrameLive.set(true);
                }
            };
            webContents.addObserver(observer);

            webContentsReference.set(webContents);
        });
        CriteriaHelper.pollUiThread(
                () -> isRenderFrameLive.get(), "Spare renderer is not initialized");
        PostTask.runOrPostTask(
                UiThreadTaskTraits.DEFAULT, () -> webContentsReference.get().destroy());
    }

    /** Tests that taking a spare WebContents makes it unavailable to subsequent callers. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testTakeSpareWebContents() {
        mWarmupManager.createSpareWebContents();
        WebContents webContents = mWarmupManager.takeSpareWebContents(false, false);
        Assert.assertNotNull(webContents);
        Assert.assertFalse(mWarmupManager.hasSpareWebContents());
        webContents.destroy();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testTakeSpareWebContentsChecksArguments() {
        mWarmupManager.createSpareWebContents();
        Assert.assertNull(mWarmupManager.takeSpareWebContents(true, false));
        Assert.assertNull(mWarmupManager.takeSpareWebContents(true, true));
        Assert.assertTrue(mWarmupManager.hasSpareWebContents());
        Assert.assertNotNull(mWarmupManager.takeSpareWebContents(false, true));
        Assert.assertFalse(mWarmupManager.hasSpareWebContents());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testClearsDeadWebContents() {
        mWarmupManager.createSpareWebContents();
        WebContentsUtils.simulateRendererKilled(mWarmupManager.mSpareWebContents);
        Assert.assertNull(mWarmupManager.takeSpareWebContents(false, false));
    }

    /** Checks that the View inflation works. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testInflateLayout() {
        int layoutId = R.layout.custom_tabs_control_container;
        int toolbarId = R.layout.custom_tabs_toolbar;
        mWarmupManager.initializeViewHierarchy(mContext, layoutId, toolbarId);
        Assert.assertTrue(mWarmupManager.hasViewHierarchyWithToolbar(layoutId));
    }

    /**
     * Tests that pre-connects can be initiated from the Java side.
     *
     * @param profileParameter String value to indicate which profile to use for pre-connect. This
     *         is passed by {@link ProfileParams}.
     * @throws InterruptedException May come from tryAcquire method call.
     */
    @Test
    @SmallTest
    @UseMethodParameter(ProfileParams.class)
    public void testPreconnect(String profileParameter) throws InterruptedException {
        ProfileType profileType = ProfileType.valueOf(profileParameter);
        Profile profile = getProfile(profileType);
        EmbeddedTestServer server = new EmbeddedTestServer();
        try {
            // The predictor prepares 2 connections when asked to preconnect. Initializes the
            // semaphore to be unlocked after 2 connections.
            final Semaphore connectionsSemaphore = new Semaphore(1 - 2);

            // Cannot use EmbeddedTestServer#createAndStartServer(), as we need to add the
            // connection listener.
            server.initializeNative(mContext, EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
            server.addDefaultHandlers("");
            server.setConnectionListener(new EmbeddedTestServer.ConnectionListener() {
                @Override
                public void acceptedSocket(long socketId) {
                    connectionsSemaphore.release();
                }
            });
            server.start();

            final String url = server.getURL("/hello_world.html");
            PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                    () -> { mWarmupManager.maybePreconnectUrlAndSubResources(profile, url); });
            boolean isAcquired = connectionsSemaphore.tryAcquire(5, TimeUnit.SECONDS);
            if (profileType == ProfileType.REGULAR_PROFILE && !isAcquired) {
                // Starts at -1.
                int actualConnections = connectionsSemaphore.availablePermits() + 1;
                Assert.fail("Pre-connect failed for regular profile: Expected 2 connections, got "
                        + actualConnections);
            } else if (profileType != ProfileType.REGULAR_PROFILE && isAcquired) {
                Assert.fail("Pre-connect should fail for incognito profiles.");
            }
        } finally {
            server.stopAndDestroyServer();
        }
    }
}
