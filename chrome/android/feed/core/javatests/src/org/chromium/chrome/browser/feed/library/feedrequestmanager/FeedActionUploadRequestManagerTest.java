// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.Base64;

import com.google.protobuf.ByteString;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpResponse;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeMainThreadRunner;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeTaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.testing.FakeThreadUtils;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.testing.RequiredConsumer;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.testing.actionmanager.FakeViewActionManager;
import org.chromium.chrome.browser.feed.library.testing.network.FakeNetworkClient;
import org.chromium.chrome.browser.feed.library.testing.protocoladapter.FakeProtocolAdapter;
import org.chromium.chrome.browser.feed.library.testing.store.FakeStore;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ActionRequestProto.ActionRequest;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.FeedActionRequestProto.FeedActionRequest;
import org.chromium.components.feed.core.proto.wire.FeedActionResponseProto.FeedActionResponse;
import org.chromium.components.feed.core.proto.wire.FeedRequestProto.FeedRequest;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;
import org.chromium.components.feed.core.proto.wire.SemanticPropertiesProto.SemanticProperties;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.Set;

/** Test of the {@link FeedActionUploadRequestManager} class. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedActionUploadRequestManagerTest {
    public static final long ID = 42;
    private static final ConsistencyToken TOKEN_1 =
            ConsistencyToken.newBuilder()
                    .setToken(ByteString.copyFrom(new byte[] {0x1, 0xa}))
                    .build();
    private static final ConsistencyToken TOKEN_2 =
            ConsistencyToken.newBuilder()
                    .setToken(ByteString.copyFrom(new byte[] {0x1, 0xf}))
                    .build();
    private static final ConsistencyToken TOKEN_3 =
            ConsistencyToken.newBuilder()
                    .setToken(ByteString.copyFrom(new byte[] {0x2, 0xa}))
                    .build();
    private static final String CONTENT_ID = "contentId";
    private static final String CONTENT_ID_2 = "contentId2";
    private static final String CONTENT_ID_LONG =
            "extremely-long-content-id-that-should-take-a-lot-of-bytes";
    private static final byte[] SEMANTIC_PROPERTIES_BYTES = new byte[] {0x1, 0xa};
    private static final SemanticProperties SEMANTIC_PROPERTIES =
            SemanticProperties.newBuilder()
                    .setSemanticPropertiesData(ByteString.copyFrom(SEMANTIC_PROPERTIES_BYTES))
                    .build();
    private static final Response RESPONSE_1 =
            Response.newBuilder()
                    .setExtension(FeedActionResponse.feedActionResponse,
                            FeedActionResponse.newBuilder().setConsistencyToken(TOKEN_2).build())
                    .build();
    private static final Response RESPONSE_2 =
            Response.newBuilder()
                    .setExtension(FeedActionResponse.feedActionResponse,
                            FeedActionResponse.newBuilder().setConsistencyToken(TOKEN_3).build())
                    .build();

    private final Configuration mConfiguration = new Configuration.Builder().build();
    private final FakeClock mFakeClock = new FakeClock();
    private FakeViewActionManager mFakeViewActionManager;
    private ExtensionRegistryLite mRegistry;
    private FakeNetworkClient mFakeNetworkClient;
    private FakeProtocolAdapter mFakeProtocolAdapter;
    private FakeStore mFakeStore;
    private FakeMainThreadRunner mFakeMainThreadRunner = FakeMainThreadRunner.runTasksImmediately();
    private FakeTaskQueue mFakeTaskQueue;
    private FakeThreadUtils mFakeThreadUtils;
    private FeedActionUploadRequestManager mRequestManager;
    private RequiredConsumer<Result<ConsistencyToken>> mConsumer;

    @Before
    public void setUp() {
        initMocks(this);
        mRegistry = ExtensionRegistryLite.newInstance();
        mRegistry.add(FeedRequest.feedRequest);
        mRegistry.add(FeedActionRequest.feedActionRequest);
        mFakeThreadUtils = FakeThreadUtils.withThreadChecks();
        mFakeNetworkClient = new FakeNetworkClient(mFakeThreadUtils);
        mFakeTaskQueue = new FakeTaskQueue(mFakeClock, mFakeThreadUtils);
        mFakeProtocolAdapter = new FakeProtocolAdapter();
        mFakeStore = new FakeStore(mConfiguration, mFakeThreadUtils, mFakeTaskQueue, mFakeClock);
        mFakeViewActionManager = new FakeViewActionManager(mFakeStore);
        mConsumer = new RequiredConsumer<>(input -> { mFakeThreadUtils.checkNotMainThread(); });

        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", VERSION_CODES.KITKAT);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "RELEASE", "4.4.3");
        ReflectionHelpers.setStaticField(Build.class, "CPU_ABI", "armeabi");
        ReflectionHelpers.setStaticField(Build.class, "TAGS", "dev-keys");
        mRequestManager = createRequestManager(mConfiguration);
        mFakeThreadUtils.enforceMainThread(false);
        mFakeTaskQueue.initialize(() -> {});
    }

    @Test
    public void testTriggerUploadActions_ttlExceededRemove() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 5L)
                        .build();
        mRequestManager = createRequestManager(configuration);
        StreamUploadableAction action = StreamUploadableAction.newBuilder()
                                                .setUploadAttempts(1)
                                                .setTimestampSeconds(1L)
                                                .setFeatureContentId(CONTENT_ID)
                                                .build();
        mFakeStore.setStreamUploadableActions(action);
        mFakeClock.set(5000L);
        mFakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        mRequestManager.triggerUploadActions(
                setOf(action), ConsistencyToken.getDefaultInstance(), mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
        assertThat(mFakeStore.getContentById(CONTENT_ID)).isEmpty();
    }

    @Test
    public void testTriggerUploadActions_maxUploadsRemove() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .build();
        mRequestManager = createRequestManager(configuration);
        StreamUploadableAction action = StreamUploadableAction.newBuilder()
                                                .setUploadAttempts(2)
                                                .setFeatureContentId(CONTENT_ID)
                                                .build();
        mFakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        mRequestManager.triggerUploadActions(
                setOf(action), ConsistencyToken.getDefaultInstance(), mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
        assertThat(mFakeStore.getContentById(CONTENT_ID)).isEmpty();
    }

    @Test
    public void testTriggerUploadActions_batchSuccess() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 1L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 2L)
                        .build();
        mRequestManager = createRequestManager(configuration);

        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        mConsumer = new RequiredConsumer<>(input -> {
            mFakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isTrue();
            assertThat(input.getValue().toByteArray()).isEqualTo(TOKEN_3.toByteArray());
        });
        mFakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_1))
                .addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_2));
        mRequestManager.triggerUploadActions(actionSet, TOKEN_1, mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_batchFirstFailure() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 2L)
                        .build();
        mRequestManager = createRequestManager(configuration);
        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        mConsumer = new RequiredConsumer<>(input -> {
            mFakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isFalse();
        });
        mFakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 500, RESPONSE_1));
        mRequestManager.triggerUploadActions(actionSet, TOKEN_1, mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_batchFirstSuccessSecondFailure() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 2L)
                        .build();
        mRequestManager = createRequestManager(configuration);
        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        mConsumer = new RequiredConsumer<>(input -> {
            mFakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isTrue();
            assertThat(input.getValue().toByteArray()).isEqualTo(TOKEN_2.toByteArray());
        });
        mFakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_1))
                .addResponse(createHttpResponse(/* responseCode= */ 500, RESPONSE_2));
        mRequestManager.triggerUploadActions(actionSet, TOKEN_1, mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_batchFirstReachesMaxNumActions() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 1L)
                        .build();
        mRequestManager = createRequestManager(configuration);
        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        mConsumer = new RequiredConsumer<>(input -> {
            mFakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isTrue();
            assertThat(input.getValue().toByteArray()).isEqualTo(TOKEN_2.toByteArray());
        });
        mFakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_1));
        mRequestManager.triggerUploadActions(actionSet, TOKEN_1, mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_batchFirstReachesMaxSize() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 1L)
                        .build();
        mRequestManager = createRequestManager(configuration);
        Set<StreamUploadableAction> actionSet = orderedSetOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_LONG).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build());
        mConsumer = new RequiredConsumer<>(input -> {
            mFakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isTrue();
            assertThat(input.getValue().toByteArray()).isEqualTo(TOKEN_2.toByteArray());
        });
        mFakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_1));
        mRequestManager.triggerUploadActions(actionSet, TOKEN_1, mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_batchNoUploadableActions() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 1L)
                        .build();
        mRequestManager = createRequestManager(configuration);
        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_LONG).build());
        mConsumer = new RequiredConsumer<>(input -> {
            mFakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isFalse();
        });
        mRequestManager.triggerUploadActions(actionSet, TOKEN_1, mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_getMethod() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 2L)
                        .build();
        mRequestManager = createRequestManager(configuration);
        StreamUploadableAction action =
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build();
        Set<StreamUploadableAction> actionSet = setOf(action);
        mFakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        mRequestManager.triggerUploadActions(
                actionSet, ConsistencyToken.getDefaultInstance(), mConsumer);

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();
        assertHttpRequestFormattedCorrectly(httpRequest);

        ActionRequest request = getActionRequestFromHttpRequest(httpRequest);
        UploadableActionsRequestBuilder builder =
                new UploadableActionsRequestBuilder(mFakeProtocolAdapter);
        ActionRequest expectedRequest =
                builder.setConsistencyToken(ConsistencyToken.getDefaultInstance())
                        .setActions(actionSet)
                        .build();
        assertThat(request.toByteArray()).isEqualTo(expectedRequest.toByteArray());
        assertThat(mConsumer.isCalled()).isTrue();
        assertThat(mFakeStore.getContentById(CONTENT_ID))
                .contains(StreamUploadableAction.newBuilder()
                                  .setFeatureContentId(CONTENT_ID)
                                  .setUploadAttempts(1)
                                  .build());
    }

    @Test
    public void testTriggerUploadActions_defaultMethod() throws Exception {
        Set<StreamUploadableAction> actionSet =
                setOf(StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build());
        mFakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        mRequestManager.triggerUploadActions(
                actionSet, ConsistencyToken.getDefaultInstance(), mConsumer);

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();

        ActionRequest request = getActionRequestFromHttpRequestBody(httpRequest);
        UploadableActionsRequestBuilder builder =
                new UploadableActionsRequestBuilder(mFakeProtocolAdapter);
        Set<StreamUploadableAction> expectedActionSet =
                setOf(StreamUploadableAction.newBuilder()
                                .setFeatureContentId(CONTENT_ID)
                                .setUploadAttempts(1)
                                .build());
        ActionRequest expectedRequest =
                builder.setConsistencyToken(ConsistencyToken.getDefaultInstance())
                        .setActions(expectedActionSet)
                        .build();
        assertThat(request.toByteArray()).isEqualTo(expectedRequest.toByteArray());

        assertThat(mConsumer.isCalled()).isTrue();
    }

    @Test
    public void testTriggerUploadActions_withSemanticProperties() throws Exception {
        mFakeStore.addSemanticProperties(CONTENT_ID, SEMANTIC_PROPERTIES);
        Set<StreamUploadableAction> actionSet =
                setOf(StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build());
        mFakeNetworkClient.addResponse(
                createHttpResponse(/* responseCode= */ 200, Response.getDefaultInstance()));
        mRequestManager.triggerUploadActions(
                actionSet, ConsistencyToken.getDefaultInstance(), mConsumer);

        HttpRequest httpRequest = mFakeNetworkClient.getLatestRequest();

        ActionRequest request = getActionRequestFromHttpRequestBody(httpRequest);
        assertThat(request.getExtension(FeedActionRequest.feedActionRequest)
                           .getFeedActionList()
                           .get(0)
                           .getSemanticProperties()
                           .getSemanticPropertiesData())
                .isEqualTo(SEMANTIC_PROPERTIES.getSemanticPropertiesData());
    }

    @Test
    public void testTriggerUploadAllActions() throws Exception {
        Configuration configuration =
                new Configuration.Builder()
                        .put(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.GET)
                        .put(ConfigKey.FEED_ACTION_TTL_SECONDS, 1000L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 20L)
                        .put(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 1L)
                        .put(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 2L)
                        .build();
        mRequestManager = createRequestManager(configuration);

        Set<StreamUploadableAction> actionSet = setOf(
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID).build(),
                StreamUploadableAction.newBuilder().setFeatureContentId(CONTENT_ID_2).build());
        mFakeViewActionManager.mViewActions.addAll(actionSet);
        mConsumer = new RequiredConsumer<>(input -> {
            mFakeThreadUtils.checkNotMainThread();
            assertThat(input.isSuccessful()).isTrue();
            assertThat(input.getValue().toByteArray()).isEqualTo(TOKEN_3.toByteArray());
        });
        mFakeNetworkClient.addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_1))
                .addResponse(createHttpResponse(/* responseCode= */ 200, RESPONSE_2));
        mRequestManager.triggerUploadAllActions(TOKEN_1, mConsumer);

        assertThat(mConsumer.isCalled()).isTrue();
    }

    private static void assertHttpRequestFormattedCorrectly(HttpRequest httpRequest) {
        assertThat(httpRequest.getBody()).hasLength(0);
        assertThat(httpRequest.getMethod()).isEqualTo(HttpMethod.GET);
        assertThat(httpRequest.getUri().getQueryParameter("fmt")).isEqualTo("bin");
        assertThat(httpRequest.getUri().getQueryParameter(RequestHelper.MOTHERSHIP_PARAM_PAYLOAD))
                .isNotNull();
    }

    private static HttpResponse createHttpResponse(int responseCode, Response response)
            throws IOException {
        byte[] rawResponse = response.toByteArray();
        ByteBuffer buffer = ByteBuffer.allocate(rawResponse.length + (Integer.SIZE / 8));
        CodedOutputStream codedOutputStream = CodedOutputStream.newInstance(buffer);
        codedOutputStream.writeUInt32NoTag(rawResponse.length);
        codedOutputStream.writeRawBytes(rawResponse);
        codedOutputStream.flush();
        return new HttpResponse(responseCode, buffer.array(), false);
    }

    private ActionRequest getActionRequestFromHttpRequest(HttpRequest httpRequest)
            throws Exception {
        return ActionRequest.parseFrom(
                Base64.decode(httpRequest.getUri().getQueryParameter(
                                      RequestHelper.MOTHERSHIP_PARAM_PAYLOAD),
                        Base64.URL_SAFE),
                mRegistry);
    }

    private ActionRequest getActionRequestFromHttpRequestBody(HttpRequest httpRequest)
            throws Exception {
        return ActionRequest.parseFrom(httpRequest.getBody(), mRegistry);
    }

    private FeedActionUploadRequestManager createRequestManager(Configuration configuration) {
        return new FeedActionUploadRequestManager(mFakeViewActionManager, configuration,
                mFakeNetworkClient, mFakeProtocolAdapter, new FeedExtensionRegistry(ArrayList::new),
                mFakeMainThreadRunner, mFakeTaskQueue, mFakeThreadUtils, mFakeStore, mFakeClock);
    }

    private static <T> Set<T> setOf(T... items) {
        Set<T> result = new HashSet<>();
        Collections.addAll(result, items);
        return result;
    }

    private static <T> Set<T> orderedSetOf(T... items) {
        Set<T> result = new LinkedHashSet<>();
        Collections.addAll(result, items);
        return result;
    }
}
