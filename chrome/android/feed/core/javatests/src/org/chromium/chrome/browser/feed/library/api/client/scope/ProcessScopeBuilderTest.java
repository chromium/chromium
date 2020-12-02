// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.scope;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;

import com.google.common.util.concurrent.MoreExecutors;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.config.DebugBehavior;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.api.host.proto.ProtoExtensionProvider;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorage;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipSupportedApi;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProviderJni;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;

/** Tests for {@link ProcessScopeBuilder}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
public class ProcessScopeBuilderTest {
    // Mocks for required fields
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private NetworkClient mNetworkClient;
    @Mock
    private SchedulerApi mSchedulerApi;
    @Mock
    private ApplicationInfo mApplicationInfo;
    @Mock
    private TooltipSupportedApi mTooltipSupportedApi;
    @Mock
    private IdentityServicesProvider.Natives mIdentityServicesProviderJniMock;
    @Mock
    private Profile mProfileMock;
    @Mock
    private IdentityManager mIdentifiyManagerMock;

    // Mocks for optional fields
    @Mock
    private ThreadUtils mThreadUtils;

    private final ProtoExtensionProvider mProtoExtensionProvider = ArrayList::new;
    private Configuration mConfiguration = new Configuration.Builder().build();
    private Context mContext;

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Before
    public void setUp() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        Profile.setLastUsedProfileForTesting(mProfileMock);
        jniMocker.mock(IdentityServicesProviderJni.TEST_HOOKS, mIdentityServicesProviderJniMock);
        when(mIdentityServicesProviderJniMock.getIdentityManager(mProfileMock))
                .thenReturn(mIdentifiyManagerMock);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    public void testBasicBuild() {
        // No crash should happen.
        ProcessScope processScope =
                new ProcessScopeBuilder(mConfiguration, MoreExecutors.newDirectExecutorService(),
                        mBasicLoggingApi, mNetworkClient, mSchedulerApi, DebugBehavior.VERBOSE,
                        mContext, mApplicationInfo, mTooltipSupportedApi)
                        .setJournalStorageDirect(mock(JournalStorageDirect.class))
                        .setContentStorageDirect(mock(ContentStorageDirect.class))
                        .build();

        assertThat(processScope.getRequestManager()).isNotNull();
        assertThat(processScope.getAppLifecycleListener()).isNotNull();
        assertThat(processScope.getKnownContent()).isNotNull();
        assertThat(processScope.getAppLifecycleListener()).isNotNull();
    }

    @Test
    public void testComplexBuild() {
        // No crash should happen.
        ProcessScope processScope =
                new ProcessScopeBuilder(mConfiguration, MoreExecutors.newDirectExecutorService(),
                        mBasicLoggingApi, mNetworkClient, mSchedulerApi, DebugBehavior.VERBOSE,
                        mContext, mApplicationInfo, mTooltipSupportedApi)
                        .setProtoExtensionProvider(mProtoExtensionProvider)
                        .setJournalStorageDirect(mock(JournalStorageDirect.class))
                        .setContentStorageDirect(mock(ContentStorageDirect.class))
                        .build();

        assertThat(processScope.getRequestManager()).isNotNull();
        assertThat(processScope.getAppLifecycleListener()).isNotNull();
        assertThat(processScope.getAppLifecycleListener()).isNotNull();
    }

    @Test
    public void testDirectStorage() {
        ContentStorageDirect contentStorageDirect = mock(ContentStorageDirect.class);
        JournalStorageDirect journalStorageDirect = mock(JournalStorageDirect.class);
        ProcessScopeBuilder builder =
                new ProcessScopeBuilder(mConfiguration, MoreExecutors.newDirectExecutorService(),
                        mBasicLoggingApi, mNetworkClient, mSchedulerApi, DebugBehavior.VERBOSE,
                        mContext, mApplicationInfo, mTooltipSupportedApi)
                        .setContentStorageDirect(contentStorageDirect)
                        .setJournalStorageDirect(journalStorageDirect);
        assertThat(builder.mContentStorage).isEqualTo(contentStorageDirect);
        assertThat(builder.mJournalStorage).isEqualTo(journalStorageDirect);
    }

    @Test
    public void testStorage_direct() {
        mConfiguration =
                new Configuration.Builder().put(ConfigKey.USE_DIRECT_STORAGE, true).build();
        ContentStorage contentStorageDirect = mock(
                ContentStorage.class, withSettings().extraInterfaces(ContentStorageDirect.class));
        JournalStorage journalStorageDirect = mock(
                JournalStorage.class, withSettings().extraInterfaces(JournalStorageDirect.class));
        ProcessScopeBuilder builder =
                new ProcessScopeBuilder(mConfiguration, MoreExecutors.newDirectExecutorService(),
                        mBasicLoggingApi, mNetworkClient, mSchedulerApi, DebugBehavior.VERBOSE,
                        mContext, mApplicationInfo, mTooltipSupportedApi)
                        .setContentStorage(contentStorageDirect)
                        .setJournalStorage(journalStorageDirect);
        MainThreadRunner mainThreadRunner = new MainThreadRunner();
        assertThat(builder.buildContentStorage(mainThreadRunner)).isEqualTo(contentStorageDirect);
        assertThat(builder.buildJournalStorage(mainThreadRunner)).isEqualTo(journalStorageDirect);
    }

    @Test
    public void testStorage_wrapped() {
        mConfiguration =
                new Configuration.Builder().put(ConfigKey.USE_DIRECT_STORAGE, false).build();
        ContentStorage contentStorageDirect = mock(
                ContentStorage.class, withSettings().extraInterfaces(ContentStorageDirect.class));
        JournalStorage journalStorageDirect = mock(
                JournalStorage.class, withSettings().extraInterfaces(JournalStorageDirect.class));
        ProcessScopeBuilder builder =
                new ProcessScopeBuilder(mConfiguration, MoreExecutors.newDirectExecutorService(),
                        mBasicLoggingApi, mNetworkClient, mSchedulerApi, DebugBehavior.VERBOSE,
                        mContext, mApplicationInfo, mTooltipSupportedApi)
                        .setContentStorage(contentStorageDirect)
                        .setJournalStorage(journalStorageDirect);
        assertThat(builder.mContentStorage).isNotEqualTo(contentStorageDirect);
        assertThat(builder.mJournalStorage).isNotEqualTo(journalStorageDirect);
    }
}
