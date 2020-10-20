// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.common.testing;

import static org.mockito.Mockito.mock;

import com.google.protobuf.GeneratedMessageLite.GeneratedExtension;

import org.chromium.chrome.browser.feed.library.api.client.lifecycle.AppLifecycleListener;
import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.proto.ProtoExtensionProvider;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.storage.ContentStorageDirect;
import org.chromium.chrome.browser.feed.library.api.host.storage.JournalStorageDirect;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelProviderFactory;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.scope.ClearAllListener;
import org.chromium.chrome.browser.feed.library.api.internal.sessionmanager.FeedSessionManager;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeDirectExecutor;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedAppLifecycleListener;
import org.chromium.chrome.browser.feed.library.feedmodelprovider.FeedModelProviderFactory;
import org.chromium.chrome.browser.feed.library.feedprotocoladapter.FeedProtocolAdapter;
import org.chromium.chrome.browser.feed.library.feedrequestmanager.RequestManagerImpl;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.FeedSessionManagerFactory;
import org.chromium.chrome.browser.feed.library.feedsessionmanager.FeedSessionManagerImpl;
import org.chromium.chrome.browser.feed.library.feedstore.FeedStore;
import org.chromium.chrome.browser.feed.library.hostimpl.storage.testing.InMemoryContentStorage;
import org.chromium.chrome.browser.feed.library.hostimpl.storage.testing.InMemoryJournalStorage;
import org.chromium.chrome.browser.feed.library.sharedstream.piet.PietRequiredContentAdapter;
import org.chromium.chrome.browser.feed.library.testing.actionmanager.FakeViewActionManager;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.chrome.browser.feed.library.testing.host.scheduler.FakeSchedulerApi;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeActionUploadRequestManager;
import org.chromium.chrome.browser.feed.library.testing.requestmanager.FakeFeedRequestManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * This is a Scope type object which is used in the Infrastructure Integration tests. It sets the
 * Feed objects from ProtocolAdapter through the FeedSessionManager.
 */
public class InfraIntegrationScope {
    private static final boolean USE_TIMEOUT_SCHEDULER = true;

    /**
     * For the TimeoutSession tests, this is how long we allow it to run before declaring a timeout
     * error.
     */
    public static final long TIMEOUT_TEST_TIMEOUT = TimeUnit.SECONDS.toMillis(20);

    private final Configuration mConfiguration;
    private final ContentStorageDirect mContentStorage;
    private final FakeClock mFakeClock;
    private final FakeDirectExecutor mFakeDirectExecutor;
    private final FakeMainThreadRunner mFakeMainThreadRunner;
    private final FakeFeedRequestManager mFakeFeedRequestManager;
    private final FakeThreadUtils mFakeThreadUtils;
    private final FeedAppLifecycleListener mAppLifecycleListener;
    private final FeedModelProviderFactory mModelProviderFactory;
    private final FeedProtocolAdapter mFeedProtocolAdapter;
    private final FeedSessionManagerImpl mFeedSessionManager;
    private final FeedStore mStore;
    private final JournalStorageDirect mJournalStorage;
    private final SchedulerApi mSchedulerApi;
    private final TaskQueue mTaskQueue;
    private final RequestManager mRequestManager;

    private InfraIntegrationScope(FakeThreadUtils fakeThreadUtils,
            FakeDirectExecutor fakeDirectExecutor, SchedulerApi schedulerApi, FakeClock fakeClock,
            Configuration configuration, ContentStorageDirect contentStorage,
            JournalStorageDirect journalStorage, FakeMainThreadRunner fakeMainThreadRunner) {
        this.mFakeClock = fakeClock;
        this.mConfiguration = configuration;
        this.mContentStorage = contentStorage;
        this.mJournalStorage = journalStorage;
        this.mFakeDirectExecutor = fakeDirectExecutor;
        this.mFakeMainThreadRunner = fakeMainThreadRunner;
        this.mSchedulerApi = schedulerApi;
        this.mFakeThreadUtils = fakeThreadUtils;
        TimingUtils timingUtils = new TimingUtils();
        mAppLifecycleListener = new FeedAppLifecycleListener(fakeThreadUtils);
        FakeBasicLoggingApi fakeBasicLoggingApi = new FakeBasicLoggingApi();

        FeedExtensionRegistry extensionRegistry =
                new FeedExtensionRegistry(new ExtensionProvider());
        mTaskQueue = new TaskQueue(
                fakeBasicLoggingApi, fakeDirectExecutor, fakeMainThreadRunner, fakeClock);
        mStore = new FeedStore(configuration, timingUtils, extensionRegistry, contentStorage,
                journalStorage, fakeThreadUtils, mTaskQueue, fakeClock, fakeBasicLoggingApi,
                fakeMainThreadRunner);
        mFeedProtocolAdapter = new FeedProtocolAdapter(
                Collections.singletonList(new PietRequiredContentAdapter()), timingUtils);
        mFakeFeedRequestManager = new FakeFeedRequestManager(
                fakeThreadUtils, fakeMainThreadRunner, mFeedProtocolAdapter, mTaskQueue);
        FakeActionUploadRequestManager fakeActionUploadRequestManager =
                new FakeActionUploadRequestManager(mStore, new FakeViewActionManager(mStore),
                        FakeThreadUtils.withThreadChecks());
        ActionManager actionManager = mock(ActionManager.class);
        mFeedSessionManager = new FeedSessionManagerFactory(mTaskQueue, mStore, timingUtils,
                fakeThreadUtils, mFeedProtocolAdapter, mFakeFeedRequestManager,
                fakeActionUploadRequestManager, schedulerApi, configuration, fakeClock,
                mAppLifecycleListener, fakeMainThreadRunner, fakeBasicLoggingApi, actionManager)
                                      .create();
        new ClearAllListener(
                mTaskQueue, mFeedSessionManager, mStore, fakeThreadUtils, mAppLifecycleListener);
        mFeedSessionManager.initialize();
        mModelProviderFactory = new FeedModelProviderFactory(mFeedSessionManager, fakeThreadUtils,
                timingUtils, mTaskQueue, fakeMainThreadRunner, configuration, fakeBasicLoggingApi);
        mRequestManager = new RequestManagerImpl(mFakeFeedRequestManager, mFeedSessionManager);
    }

    public ProtocolAdapter getProtocolAdapter() {
        return mFeedProtocolAdapter;
    }

    public FeedSessionManager getFeedSessionManager() {
        return mFeedSessionManager;
    }

    public ModelProviderFactory getModelProviderFactory() {
        return mModelProviderFactory;
    }

    public FakeClock getFakeClock() {
        return mFakeClock;
    }

    public FakeDirectExecutor getFakeDirectExecutor() {
        return mFakeDirectExecutor;
    }

    public FakeMainThreadRunner getFakeMainThreadRunner() {
        return mFakeMainThreadRunner;
    }

    public FakeThreadUtils getFakeThreadUtils() {
        return mFakeThreadUtils;
    }

    public FeedStore getStore() {
        return mStore;
    }

    public TaskQueue getTaskQueue() {
        return mTaskQueue;
    }

    public FakeFeedRequestManager getFakeFeedRequestManager() {
        return mFakeFeedRequestManager;
    }

    public AppLifecycleListener getAppLifecycleListener() {
        return mAppLifecycleListener;
    }

    public RequestManager getRequestManager() {
        return mRequestManager;
    }

    @Override
    public InfraIntegrationScope clone() {
        return new InfraIntegrationScope(mFakeThreadUtils, mFakeDirectExecutor, mSchedulerApi,
                mFakeClock, mConfiguration, mContentStorage, mJournalStorage,
                mFakeMainThreadRunner);
    }

    private static class ExtensionProvider implements ProtoExtensionProvider {
        @Override
        public List<GeneratedExtension<?, ?>> getProtoExtensions() {
            return new ArrayList<>();
        }
    }

    /** Builder for creating the {@link InfraIntegrationScope} */
    public static class Builder {
        private final FakeClock mFakeClock = new FakeClock();
        private final FakeMainThreadRunner mFakeMainThreadRunner =
                FakeMainThreadRunner.create(mFakeClock);
        private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();

        private Configuration mConfiguration = Configuration.getDefaultInstance();
        private FakeDirectExecutor mFakeDirectExecutor =
                FakeDirectExecutor.runTasksImmediately(mFakeThreadUtils);
        private SchedulerApi mSchedulerApi1 = new FakeSchedulerApi(mFakeThreadUtils);

        public Builder() {}

        public Builder setConfiguration(Configuration configuration) {
            this.mConfiguration = configuration;
            return this;
        }

        public Builder setSchedulerApi(SchedulerApi schedulerApi) {
            this.mSchedulerApi1 = schedulerApi;
            return this;
        }

        public Builder withQueuingTasks() {
            mFakeDirectExecutor = FakeDirectExecutor.queueAllTasks(mFakeThreadUtils);
            return this;
        }

        public Builder withTimeoutSessionConfiguration(long timeoutMs) {
            mConfiguration = mConfiguration.toBuilder()
                                     .put(ConfigKey.USE_TIMEOUT_SCHEDULER, USE_TIMEOUT_SCHEDULER)
                                     .put(ConfigKey.TIMEOUT_TIMEOUT_MS, timeoutMs)
                                     .build();
            return this;
        }

        public InfraIntegrationScope build() {
            return new InfraIntegrationScope(mFakeThreadUtils, mFakeDirectExecutor, mSchedulerApi1,
                    mFakeClock, mConfiguration, new InMemoryContentStorage(),
                    new InMemoryJournalStorage(), mFakeMainThreadRunner);
        }
    }
}
