// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.client.scope;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

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
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionReader;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.knowncontent.FeedKnownContent;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.ActionUploadRequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.FeedRequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.scope.ClearAllListener;
import org.chromium.chrome.browser.feed.library.api.internal.scope.FeedProcessScope;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.Validators;
import org.chromium.chrome.browser.feed.library.common.concurrent.DirectHostSupported;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.chrome.browser.feed.library.common.time.SystemClockImpl;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.feedactionmanager.FeedActionManagerImpl;
import org.chromium.chrome.browser.feed.library.feedactionreader.FeedActionReader;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedAppLifecycleListener;
import org.chromium.chrome.browser.feed.library.feedknowncontent.FeedKnownContentImpl;
import org.chromium.chrome.browser.feed.library.feedprotocoladapter.FeedProtocolAdapter;
import org.chromium.chrome.browser.feed.library.feedrequestmanager.FeedActionUploadRequestManager;
import org.chromium.chrome.browser.feed.library.feedrequestmanager.FeedRequestManagerImpl;
import org.chromium.chrome.browser.feed.library.feedrequestmanager.RequestManagerImpl;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.FeedSessionManagerFactory;
import org.chromium.chrome.browser.feed.library.feedstore.ContentStorageDirectImpl;
import org.chromium.chrome.browser.feed.library.feedstore.FeedStore;
import org.chromium.chrome.browser.feed.library.feedstore.JournalStorageDirectImpl;
import org.chromium.chrome.browser.feed.library.hostimpl.network.NetworkClientWrapper;
import org.chromium.chrome.browser.feed.library.hostimpl.scheduler.SchedulerApiWrapper;
import org.chromium.chrome.browser.feed.library.sharedstream.piet.PietRequiredContentAdapter;

import java.util.ArrayList;
import java.util.Collections;
import java.util.concurrent.Executor;

/** Creates an instance of {@link ProcessScope} */
public final class ProcessScopeBuilder {
    // Required fields.
    private final Configuration mConfiguration;
    private final Executor mSequencedExecutor;
    private final BasicLoggingApi mBasicLoggingApi;
    private final TooltipSupportedApi mTooltipSupportedApi;
    private final NetworkClient mUnwrappedNetworkClient;
    private final SchedulerApi mUnwrappedSchedulerApi;
    private final DebugBehavior mDebugBehavior;
    private final Context mContext;
    private final ApplicationInfo mApplicationInfo;

    // Optional fields - if they are not provided, we will use default implementations.
    private ProtoExtensionProvider mProtoExtensionProvider;
    private Clock mClock;

    // Either contentStorage or rawContentStorage must be provided.
    ContentStorageDirect mContentStorage;
    private ContentStorage mRawContentStorage;

    // Either journalStorage or rawJournalStorage must be provided.
    JournalStorageDirect mJournalStorage;
    private JournalStorage mRawJournalStorage;

    /** The APIs are all required to construct the scope. */
    public ProcessScopeBuilder(Configuration configuration, Executor sequencedExecutor,
            BasicLoggingApi basicLoggingApi, NetworkClient networkClient, SchedulerApi schedulerApi,
            DebugBehavior debugBehavior, Context context, ApplicationInfo applicationInfo,
            TooltipSupportedApi tooltipSupportedApi) {
        this.mConfiguration = configuration;
        this.mSequencedExecutor = sequencedExecutor;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mDebugBehavior = debugBehavior;
        this.mContext = context;
        this.mApplicationInfo = applicationInfo;
        this.mUnwrappedNetworkClient = networkClient;
        this.mUnwrappedSchedulerApi = schedulerApi;
        this.mTooltipSupportedApi = tooltipSupportedApi;
    }

    public ProcessScopeBuilder setProtoExtensionProvider(
            ProtoExtensionProvider protoExtensionProvider) {
        this.mProtoExtensionProvider = protoExtensionProvider;
        return this;
    }

    public ProcessScopeBuilder setContentStorage(ContentStorage contentStorage) {
        mRawContentStorage = contentStorage;
        return this;
    }

    public ProcessScopeBuilder setContentStorageDirect(ContentStorageDirect contentStorage) {
        this.mContentStorage = contentStorage;
        return this;
    }

    public ProcessScopeBuilder setJournalStorage(JournalStorage journalStorage) {
        mRawJournalStorage = journalStorage;
        return this;
    }

    public ProcessScopeBuilder setJournalStorageDirect(JournalStorageDirect journalStorage) {
        this.mJournalStorage = journalStorage;
        return this;
    }

    @VisibleForTesting
    ContentStorageDirect buildContentStorage(MainThreadRunner mainThreadRunner) {
        if (mContentStorage == null) {
            boolean useDirect =
                    mConfiguration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
            if (useDirect && mRawContentStorage != null
                    && mRawContentStorage instanceof ContentStorageDirect) {
                mContentStorage = (ContentStorageDirect) mRawContentStorage;
            } else if (mRawContentStorage != null) {
                mContentStorage =
                        new ContentStorageDirectImpl(mRawContentStorage, mainThreadRunner);
            } else {
                throw new IllegalStateException(
                        "one of ContentStorage, ContentStorageDirect must be provided");
            }
        }
        return mContentStorage;
    }

    @VisibleForTesting
    JournalStorageDirect buildJournalStorage(MainThreadRunner mainThreadRunner) {
        if (mJournalStorage == null) {
            boolean useDirect =
                    mConfiguration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
            if (useDirect && mRawJournalStorage != null
                    && mRawJournalStorage instanceof JournalStorageDirect) {
                mJournalStorage = (JournalStorageDirect) mRawJournalStorage;
            } else if (mRawJournalStorage != null) {
                mJournalStorage =
                        new JournalStorageDirectImpl(mRawJournalStorage, mainThreadRunner);
            } else {
                throw new IllegalStateException(
                        "one of JournalStorage, JournalStorageDirect must be provided");
            }
        }
        return mJournalStorage;
    }

    public ProcessScope build() {
        MainThreadRunner mainThreadRunner = new MainThreadRunner();
        mContentStorage = buildContentStorage(mainThreadRunner);
        mJournalStorage = buildJournalStorage(mainThreadRunner);

        ThreadUtils threadUtils = new ThreadUtils();

        boolean directHostCallEnabled =
                mConfiguration.getValueOrDefault(ConfigKey.USE_DIRECT_STORAGE, false);
        NetworkClient networkClient;
        SchedulerApi schedulerApi;
        if (mUnwrappedNetworkClient instanceof DirectHostSupported && directHostCallEnabled) {
            networkClient = mUnwrappedNetworkClient;
        } else {
            networkClient = new NetworkClientWrapper(
                    mUnwrappedNetworkClient, threadUtils, mainThreadRunner);
        }
        if (mUnwrappedSchedulerApi instanceof DirectHostSupported && directHostCallEnabled) {
            schedulerApi = mUnwrappedSchedulerApi;
        } else {
            schedulerApi =
                    new SchedulerApiWrapper(mUnwrappedSchedulerApi, threadUtils, mainThreadRunner);
        }

        // Build default component instances if necessary.
        if (mProtoExtensionProvider == null) {
            // Return an empty list of extensions by default.
            mProtoExtensionProvider = ArrayList::new;
        }
        FeedExtensionRegistry extensionRegistry =
                new FeedExtensionRegistry(mProtoExtensionProvider);
        if (mClock == null) {
            mClock = new SystemClockImpl();
        }
        TimingUtils timingUtils = new TimingUtils();
        TaskQueue taskQueue =
                new TaskQueue(mBasicLoggingApi, mSequencedExecutor, mainThreadRunner, mClock);
        FeedStore store = new FeedStore(mConfiguration, timingUtils, extensionRegistry,
                mContentStorage, mJournalStorage, threadUtils, taskQueue, mClock, mBasicLoggingApi,
                mainThreadRunner);

        FeedAppLifecycleListener lifecycleListener = new FeedAppLifecycleListener(threadUtils);
        lifecycleListener.registerObserver(store);

        ProtocolAdapter protocolAdapter = new FeedProtocolAdapter(
                Collections.singletonList(new PietRequiredContentAdapter()), timingUtils);
        ActionReader actionReader =
                new FeedActionReader(store, mClock, protocolAdapter, taskQueue, mConfiguration);
        FeedRequestManager feedRequestManager = new FeedRequestManagerImpl(mConfiguration,
                networkClient, protocolAdapter, extensionRegistry, schedulerApi, taskQueue,
                timingUtils, threadUtils, actionReader, mContext, mApplicationInfo,
                mainThreadRunner, mBasicLoggingApi, mTooltipSupportedApi);
        FeedActionManagerImpl actionManager = new FeedActionManagerImpl(
                store, threadUtils, taskQueue, mainThreadRunner, mClock, mBasicLoggingApi);
        ActionUploadRequestManager actionUploadRequestManager = new FeedActionUploadRequestManager(
                actionManager, mConfiguration, networkClient, protocolAdapter, extensionRegistry,
                mainThreadRunner, taskQueue, threadUtils, store, mClock);
        FeedSessionManagerFactory fsmFactory = new FeedSessionManagerFactory(taskQueue, store,
                timingUtils, threadUtils, protocolAdapter, feedRequestManager,
                actionUploadRequestManager, schedulerApi, mConfiguration, mClock, lifecycleListener,
                mainThreadRunner, mBasicLoggingApi, actionManager);
        FeedSessionManager feedSessionManager = fsmFactory.create();
        actionManager.initialize(feedSessionManager);
        RequestManagerImpl clientRequestManager =
                new RequestManagerImpl(feedRequestManager, feedSessionManager);

        FeedKnownContent feedKnownContent =
                new FeedKnownContentImpl(feedSessionManager, mainThreadRunner, threadUtils);

        ClearAllListener clearAllListener = new ClearAllListener(
                taskQueue, feedSessionManager, store, threadUtils, lifecycleListener);
        return new FeedProcessScope(mBasicLoggingApi, networkClient,
                Validators.checkNotNull(protocolAdapter),
                Validators.checkNotNull(clientRequestManager),
                Validators.checkNotNull(feedSessionManager), store, timingUtils, threadUtils,
                taskQueue, mainThreadRunner, lifecycleListener, mClock, mDebugBehavior,
                actionManager, mConfiguration, feedKnownContent, extensionRegistry,
                clearAllListener, mTooltipSupportedApi, mApplicationInfo);
    }
}
