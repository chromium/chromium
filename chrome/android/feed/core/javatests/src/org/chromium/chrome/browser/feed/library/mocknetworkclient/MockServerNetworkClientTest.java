// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.mocknetworkclient;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.net.Uri;

import com.google.protobuf.ByteString;
import com.google.protobuf.CodedInputStream;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.feed.library.api.host.config.ApplicationInfo;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.api.host.scheduler.SchedulerApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipSupportedApi;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionReader;
import org.chromium.chrome.browser.feed.library.api.internal.common.Model;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.MainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.TimingUtils;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.feedrequestmanager.FeedRequestManagerImpl;
import org.chromium.chrome.browser.feed.library.testing.conformance.network.NetworkClientConformanceTest;
import org.chromium.chrome.browser.feed.library.testing.host.logging.FakeBasicLoggingApi;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProviderJni;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamToken;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response.ResponseVersion;
import org.chromium.components.feed.core.proto.wire.mockserver.MockServerProto.ConditionalResponse;
import org.chromium.components.feed.core.proto.wire.mockserver.MockServerProto.MockServer;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;

/** Tests of the {@link MockServerNetworkClient} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
@Features.
DisableFeatures({ChromeFeatureList.REPORT_FEED_USER_ACTIONS, ChromeFeatureList.INTEREST_FEED_V2})
public class MockServerNetworkClientTest extends NetworkClientConformanceTest {
    private final Configuration mConfiguration = new Configuration.Builder().build();
    private final FakeClock mFakeClock = new FakeClock();
    private final FakeThreadUtils mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
    private final FeedExtensionRegistry mExtensionRegistry =
            new FeedExtensionRegistry(ArrayList::new);
    private final TimingUtils mTimingUtils = new TimingUtils();

    @Mock
    private ActionReader mActionReader;
    @Mock
    private ProtocolAdapter mProtocolAdapter;
    @Mock
    private SchedulerApi mScheduler;
    @Mock
    private TooltipSupportedApi mTooltipSupportedApi;
    @Mock
    private IdentityServicesProvider.Natives mIdentityServicesProviderJniMock;
    @Mock
    private Profile mProfileMock;
    @Mock
    private IdentityManager mIdentifiyManagerMock;
    @Captor
    private ArgumentCaptor<Response> mResponseCaptor;
    private ApplicationInfo mApplicationInfo;
    private Context mContext;
    private FakeBasicLoggingApi mBasicLoggingApi;
    private MainThreadRunner mMainThreadRunner;

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Override
    protected Uri getValidUri(@HttpMethod String method) {
        // The URI does not matter - mockNetworkClient will default to an empty response
        return new Uri.Builder().path("foo").appendPath(method).build();
    }

    @Before
    public void setUp() throws Exception {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mApplicationInfo = new ApplicationInfo.Builder(mContext).setVersionString("0").build();
        mMainThreadRunner = FakeMainThreadRunner.runTasksImmediately();

        MockServer mockServer = MockServer.getDefaultInstance();
        mNetworkClient =
                new MockServerNetworkClient(mContext, mockServer, /* responseDelayMillis= */ 0L);

        when(mActionReader.getDismissActionsWithSemanticProperties())
                .thenReturn(Result.success(Collections.emptyList()));

        mBasicLoggingApi = new FakeBasicLoggingApi();

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
    public void testSend() {
        MockServer.Builder mockServerBuilder = MockServer.newBuilder();
        Response initialResponse =
                Response.newBuilder().setResponseVersion(ResponseVersion.FEED_RESPONSE).build();
        mockServerBuilder.setInitialResponse(initialResponse);
        MockServerNetworkClient networkClient = new MockServerNetworkClient(
                mContext, mockServerBuilder.build(), /* responseDelayMillis= */ 0L);
        Consumer<HttpResponse> responseConsumer = input -> {
            try {
                CodedInputStream inputStream =
                        CodedInputStream.newInstance(input.getResponseBody());
                int length = inputStream.readRawVarint32();
                assertThat(inputStream.readRawBytes(length))
                        .isEqualTo(initialResponse.toByteArray());
                assertThat(input.getResponseCode()).isEqualTo(200);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        };
        networkClient.send(
                new HttpRequest(Uri.EMPTY, HttpMethod.POST, Collections.emptyList(), new byte[] {}),
                responseConsumer);
    }

    @Test
    public void testSend_oneTimeResponse() {
        MockServer.Builder mockServerBuilder = MockServer.newBuilder();
        Response initialResponse =
                Response.newBuilder().setResponseVersion(ResponseVersion.FEED_RESPONSE).build();
        mockServerBuilder.setInitialResponse(initialResponse);
        MockServerNetworkClient networkClient = new MockServerNetworkClient(
                mContext, mockServerBuilder.build(), /* responseDelayMillis= */ 0L);
        Consumer<HttpResponse> responseConsumer = input -> {
            try {
                CodedInputStream inputStream =
                        CodedInputStream.newInstance(input.getResponseBody());
                int length = inputStream.readRawVarint32();
                assertThat(inputStream.readRawBytes(length))
                        .isEqualTo(initialResponse.toByteArray());
                assertThat(input.getResponseCode()).isEqualTo(200);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        };
        networkClient.send(
                new HttpRequest(Uri.EMPTY, HttpMethod.POST, Collections.emptyList(), new byte[] {}),
                responseConsumer);
    }

    @Test
    public void testPaging() {
        MockServer.Builder mockServerBuilder = MockServer.newBuilder();
        Response response =
                Response.newBuilder().setResponseVersion(ResponseVersion.FEED_RESPONSE).build();
        ByteString token = ByteString.copyFromUtf8("fooToken");
        StreamToken streamToken = StreamToken.newBuilder().setNextPageToken(token).build();
        ConditionalResponse.Builder conditionalResponseBuilder = ConditionalResponse.newBuilder();
        conditionalResponseBuilder.setContinuationToken(token).setResponse(response);
        mockServerBuilder.addConditionalResponses(conditionalResponseBuilder.build());
        MockServerNetworkClient networkClient = new MockServerNetworkClient(
                mContext, mockServerBuilder.build(), /* responseDelayMillis= */ 0L);
        FeedRequestManagerImpl feedRequestManager = new FeedRequestManagerImpl(mConfiguration,
                networkClient, mProtocolAdapter, mExtensionRegistry, mScheduler, getTaskQueue(),
                mTimingUtils, mFakeThreadUtils, mActionReader, mContext, mApplicationInfo,
                mMainThreadRunner, mBasicLoggingApi, mTooltipSupportedApi);
        when(mProtocolAdapter.createModel(any(Response.class)))
                .thenReturn(Result.success(Model.empty()));

        mFakeThreadUtils.enforceMainThread(false);
        feedRequestManager.loadMore(streamToken, ConsistencyToken.getDefaultInstance(), result -> {
            assertThat(result.isSuccessful()).isTrue();
            assertThat(result.getValue().streamDataOperations).hasSize(0);
        });

        verify(mProtocolAdapter).createModel(mResponseCaptor.capture());
        assertThat(mResponseCaptor.getValue()).isEqualTo(response);
    }

    @Test
    public void testPaging_noMatch() {
        MockServer.Builder mockServerBuilder = MockServer.newBuilder();
        Response response =
                Response.newBuilder().setResponseVersion(ResponseVersion.FEED_RESPONSE).build();
        // Create a MockServerConfig without a matching token.
        ConditionalResponse.Builder conditionalResponseBuilder = ConditionalResponse.newBuilder();
        conditionalResponseBuilder.setResponse(response);
        mockServerBuilder.addConditionalResponses(conditionalResponseBuilder.build());
        MockServerNetworkClient networkClient = new MockServerNetworkClient(
                mContext, mockServerBuilder.build(), /* responseDelayMillis= */ 0L);
        FeedRequestManagerImpl feedRequestManager = new FeedRequestManagerImpl(mConfiguration,
                networkClient, mProtocolAdapter, mExtensionRegistry, mScheduler, getTaskQueue(),
                mTimingUtils, mFakeThreadUtils, mActionReader, mContext, mApplicationInfo,
                mMainThreadRunner, mBasicLoggingApi, mTooltipSupportedApi);
        when(mProtocolAdapter.createModel(any(Response.class)))
                .thenReturn(Result.success(Model.empty()));

        mFakeThreadUtils.enforceMainThread(false);
        ByteString token = ByteString.copyFromUtf8("fooToken");
        StreamToken streamToken = StreamToken.newBuilder().setNextPageToken(token).build();
        feedRequestManager.loadMore(streamToken, ConsistencyToken.getDefaultInstance(), result -> {
            assertThat(result.isSuccessful()).isTrue();
            assertThat(result.getValue().streamDataOperations).hasSize(0);
        });

        verify(mProtocolAdapter).createModel(mResponseCaptor.capture());
        assertThat(mResponseCaptor.getValue()).isEqualTo(Response.getDefaultInstance());
    }

    private FakeTaskQueue getTaskQueue() {
        FakeTaskQueue taskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        taskQueue.initialize(() -> {});
        return taskQueue;
    }
}
