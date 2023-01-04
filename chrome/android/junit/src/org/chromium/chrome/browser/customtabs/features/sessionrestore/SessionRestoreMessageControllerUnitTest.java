// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {SessionRestoreMessageControllerUnitTest.ShadowMessageDispatcherProvider.class,
                SessionRestoreMessageControllerUnitTest.ShadowSessionRestoreUtils.class,
                SessionRestoreMessageControllerUnitTest.ShadowReparentingTask.class})
public class SessionRestoreMessageControllerUnitTest {
    private static class FakeMessageDispatcher implements MessageDispatcher {
        private static PropertyModel sMessageModel;
        private boolean mWasCalled;

        @Override
        public void enqueueMessage(PropertyModel messageProperties, WebContents webContents,
                int scopeType, boolean highPriority) {
            // Not called in this test
        }

        @Override
        public void enqueueWindowScopedMessage(
                PropertyModel messageProperties, boolean highPriority) {
            mWasCalled = true;
            sMessageModel = messageProperties;
        }

        @Override
        public void dismissMessage(PropertyModel messageProperties, int dismissReason) {
            // Not called in this test
        }

        void reset() {
            mWasCalled = false;
        }

        boolean called() {
            return mWasCalled;
        }
    }

    @Implements(MessageDispatcherProvider.class)
    static class ShadowMessageDispatcherProvider {
        static FakeMessageDispatcher sMessageDispatcher;

        @Nullable
        @Implementation
        public static MessageDispatcher from(WindowAndroid windowAndroid) {
            if (windowAndroid == null) return null;
            return sMessageDispatcher;
        }
    }

    @Implements(SessionRestoreUtils.class)
    static class ShadowSessionRestoreUtils {
        private static boolean sRestorable;

        @Implementation
        protected static boolean canRestoreSession(int taskId, String url,
                SharedPreferencesManager preferences, @Nullable String clientPackage,
                @Nullable String referrer) {
            sRestorable = true;
            return true;
        }

        static boolean restorable() {
            return sRestorable;
        }

        static void reset() {
            sRestorable = false;
        }
    }

    @Implements(ReparentingTask.class)
    static class ShadowReparentingTask {
        @Nullable
        @Implementation
        protected static ReparentingTask from(Tab tab) {
            return sMockReparentingTask;
        }

        static void reset() {
            sMockReparentingTask = null;
        }
    }

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Rule
    public CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Mock
    ActivityWindowAndroid mMockWindowAndroid;
    @Mock
    ActivityLifecycleDispatcher mMockLifecycleDispatcher;
    @Mock
    BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock
    SessionRestoreManager mMockManager;
    @Mock
    Tab mMockTabInitial;
    @Mock
    Tab mMockTabRetained;

    @Captor
    private ArgumentCaptor<LifecycleObserver> mLifecycleObserverArgumentCaptor;

    static ReparentingTask sMockReparentingTask;

    private SessionRestoreMessageController mController;
    private Activity mActivity;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        sMockReparentingTask = Mockito.mock(ReparentingTask.class);
        FakeMessageDispatcher messageDispatcher = new FakeMessageDispatcher();
        ShadowMessageDispatcherProvider.sMessageDispatcher = messageDispatcher;
        ChromeFeatureList.sCctRetainableStateInMemory.setForTesting(true);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        doReturn(true).when(mMockLifecycleDispatcher).isNativeInitializationFinished();
        doReturn(mMockManager).when(env.connection).getSessionRestoreManager();
        doReturn(true).when(mMockManager).canRestoreTab();
        doReturn(mMockTabRetained).when(mMockManager).restoreTab();
        doReturn(env.tabModelSelector).when(env.tabFactory).getTabModelSelector();
        when(env.tabModelSelector.getCurrentTab()).thenReturn(mMockTabRetained);
        when(env.tabModelSelector.getCurrentTabId()).thenReturn(10);
        when(env.tabModelSelector.getTabById(10)).thenReturn(mMockTabInitial);
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
        ShadowMessageDispatcherProvider.sMessageDispatcher.reset();
        ShadowMessageDispatcherProvider.sMessageDispatcher = null;
        ShadowSessionRestoreUtils.reset();
        ShadowReparentingTask.reset();
    }

    @Test
    public void testShowMessageIfNativeLoaded() {
        mController = createSessionRestoreMessageController();
        verify(mMockLifecycleDispatcher, times(1)).isNativeInitializationFinished();
        verify(mMockManager, times(1)).canRestoreTab();
        Assert.assertTrue("Session not restorable", ShadowSessionRestoreUtils.restorable());
        Assert.assertTrue("Message not shown: MessageDispatcher never calls enqueue",
                ShadowMessageDispatcherProvider.sMessageDispatcher.called());
    }

    @Test
    public void testMessageAccepted() {
        mController = createSessionRestoreMessageController();
        Assert.assertTrue("Session not restorable", ShadowSessionRestoreUtils.restorable());
        mController.onMessageAccepted();
        verify(mMockManager, times(1)).restoreTab();
        verify(sMockReparentingTask, times(1))
                .finish(ArgumentMatchers.any(), ArgumentMatchers.any());
    }

    @Test
    public void testMessageDismissed() {
        mController = createSessionRestoreMessageController();
        Assert.assertTrue("Session not restorable", ShadowSessionRestoreUtils.restorable());
        mController.onMessageDismissed(0);
        verifyNoMoreInteractions(sMockReparentingTask);
        verify(mMockManager, times(1)).clearCache();
    }

    @Test
    public void testWaitsForNativeIfNotLoaded() {
        doReturn(false).when(mMockLifecycleDispatcher).isNativeInitializationFinished();
        mController = createSessionRestoreMessageController();
        verify(mMockLifecycleDispatcher, times(1)).isNativeInitializationFinished();
        verify(mMockLifecycleDispatcher, times(1))
                .register(mLifecycleObserverArgumentCaptor.capture());
        verify(mMockManager, times(1)).canRestoreTab();
        Assert.assertFalse("Message shown when it should not be: Native not yet initialized",
                ShadowMessageDispatcherProvider.sMessageDispatcher.called());
        mController.onFinishNativeInitialization();
        Assert.assertTrue("Message not shown: MessageDispatcher never calls enqueue",
                ShadowMessageDispatcherProvider.sMessageDispatcher.called());
        Assert.assertTrue("Session not restorable", ShadowSessionRestoreUtils.restorable());
    }

    @Test
    public void testValidPropertyModel() {
        mController = createSessionRestoreMessageController();

        Assert.assertNotNull("PropertyModel is null", FakeMessageDispatcher.sMessageModel);
        Assert.assertNotNull(FakeMessageDispatcher.sMessageModel.get(
                MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(mActivity.getResources().getString(
                                    org.chromium.chrome.R.string.restore_custom_tab_title),
                FakeMessageDispatcher.sMessageModel.get(MessageBannerProperties.TITLE));
        Assert.assertEquals(mActivity.getResources().getString(
                                    org.chromium.chrome.R.string.restore_custom_tab_description),
                FakeMessageDispatcher.sMessageModel.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(mActivity.getResources().getString(
                                    org.chromium.chrome.R.string.restore_custom_tab_button_text),
                FakeMessageDispatcher.sMessageModel.get(
                        MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertNotNull(
                FakeMessageDispatcher.sMessageModel.get(MessageBannerProperties.ON_PRIMARY_ACTION));
        Assert.assertNotNull(
                FakeMessageDispatcher.sMessageModel.get(MessageBannerProperties.ON_DISMISSED));
        Assert.assertTrue("Session not restorable", ShadowSessionRestoreUtils.restorable());
    }

    @Test
    public void testUndoMessageValidPropertyModel() {
        mController = createSessionRestoreMessageController();
        mController.showUndoMessage();

        Assert.assertNotNull("PropertyModel is null",
                SessionRestoreMessageControllerUnitTest.FakeMessageDispatcher.sMessageModel);
        Assert.assertNotNull(
                SessionRestoreMessageControllerUnitTest.FakeMessageDispatcher.sMessageModel.get(
                        MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(mActivity.getResources().getString(
                                    org.chromium.chrome.R.string.undo_restoration_title),
                SessionRestoreMessageControllerUnitTest.FakeMessageDispatcher.sMessageModel.get(
                        MessageBannerProperties.TITLE));
        Assert.assertEquals(mActivity.getResources().getString(
                                    org.chromium.chrome.R.string.restore_custom_tab_description),
                SessionRestoreMessageControllerUnitTest.FakeMessageDispatcher.sMessageModel.get(
                        MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(mActivity.getResources().getString(
                                    org.chromium.chrome.R.string.undo_restoration_button_text),
                SessionRestoreMessageControllerUnitTest.FakeMessageDispatcher.sMessageModel.get(
                        MessageBannerProperties.PRIMARY_BUTTON_TEXT));
        Assert.assertNotNull(
                SessionRestoreMessageControllerUnitTest.FakeMessageDispatcher.sMessageModel.get(
                        MessageBannerProperties.ON_PRIMARY_ACTION));
        Assert.assertNotNull(
                FakeMessageDispatcher.sMessageModel.get(MessageBannerProperties.ON_DISMISSED));
    }

    @Test
    public void testAcceptUndoMessage() {
        mController = createSessionRestoreMessageController();
        mController.onMessageAccepted();
        mController.onUndoMessageAccepted();
        verify(env.tabModelSelector, times(1)).closeTab(mMockTabRetained);
    }

    @Test
    public void testDismissUndoMessage() {
        mController = createSessionRestoreMessageController();
        mController.onMessageAccepted();
        mController.onUndoMessageDismissed(DismissReason.SECONDARY_ACTION);
        verify(env.tabModelSelector, times(1)).closeTab(mMockTabInitial);
    }

    @Test
    public void testFlagDisabledDoNothing() {
        ChromeFeatureList.sCctRetainableStateInMemory.setForTesting(false);
        mController = createSessionRestoreMessageController();
        verifyNoMoreInteractions(mMockLifecycleDispatcher);
        verifyNoMoreInteractions(mMockManager);
        Assert.assertFalse("Session is restorable even though flag is disabled",
                ShadowSessionRestoreUtils.restorable());
        Assert.assertFalse("Message shown when it should not be",
                ShadowMessageDispatcherProvider.sMessageDispatcher.called());
    }

    @Test
    public void testCannotRestoreDoNothing() {
        doReturn(false).when(mMockManager).canRestoreTab();
        mController = createSessionRestoreMessageController();
        verifyNoMoreInteractions(mMockLifecycleDispatcher);
        Assert.assertFalse("Session restorable", ShadowSessionRestoreUtils.restorable());
        Assert.assertFalse("Message shown when it should not be",
                ShadowMessageDispatcherProvider.sMessageDispatcher.called());
    }

    private SessionRestoreMessageController createSessionRestoreMessageController() {
        return new SessionRestoreMessageController(mActivity, mMockWindowAndroid,
                mMockLifecycleDispatcher, mIntentDataProvider, env.connection,
                () -> env.customTabDelegateFactory, env.tabFactory, () -> env.compositorViewHolder);
    }
}
