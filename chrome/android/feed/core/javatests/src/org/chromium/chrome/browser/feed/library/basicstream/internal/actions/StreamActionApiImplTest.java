// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.actions;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ActionType;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ContentLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipCallbackApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipCallbackApi.TooltipDismissType;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipInfo;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParser;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionSource;
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import org.chromium.chrome.browser.feed.library.sharedstream.logging.StreamContentLoggingData;
import org.chromium.chrome.browser.feed.library.sharedstream.pendingdismiss.PendingDismissCallback;
import org.chromium.chrome.browser.feed.library.testing.sharedstream.contextmenumanager.FakeContextMenuManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.ui.action.FeedActionPayloadProto.FeedActionPayload;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedAction;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.ElementType;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.Type;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.LabelledFeedActionData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.OpenContextMenuData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.OpenUrlData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.BasicLoggingMetadata;
import org.chromium.components.feed.core.proto.ui.stream.StreamStructureProto.RepresentationData;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Tests for {@link StreamActionApiImpl}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_V2)
@Features.EnableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
public class StreamActionApiImplTest {
    private static final String URL = "www.google.com";
    private static final String OPEN_LABEL = "Open";
    private static final String OPEN_IN_NEW_WINDOW_LABEL = "Open in new window";
    private static final String PARAM = "param";
    private static final String NEW_URL = "ooh.shiny.com";
    private static final int INTEREST_TYPE = 2;
    private static final ActionPayload ACTION_PAYLOAD = ActionPayload.getDefaultInstance();
    private static final LabelledFeedActionData OPEN_IN_NEW_WINDOW =
            LabelledFeedActionData.newBuilder()
                    .setLabel(OPEN_IN_NEW_WINDOW_LABEL)
                    .setFeedActionPayload(FeedActionPayload.newBuilder().setExtension(
                            FeedAction.feedActionExtension,
                            FeedAction.newBuilder()
                                    .setMetadata(
                                            FeedActionMetadata.newBuilder()
                                                    .setType(Type.OPEN_URL_NEW_WINDOW)
                                                    .setOpenUrlData(
                                                            OpenUrlData.newBuilder().setUrl(URL)))
                                    .build()))
                    .build();

    private static final String OPEN_IN_INCOGNITO_MODE_LABEL = "Open in incognito mode";

    private static final LabelledFeedActionData OPEN_IN_INCOGNITO_MODE =
            LabelledFeedActionData.newBuilder()
                    .setLabel(OPEN_IN_INCOGNITO_MODE_LABEL)
                    .setFeedActionPayload(FeedActionPayload.newBuilder().setExtension(
                            FeedAction.feedActionExtension,
                            FeedAction.newBuilder()
                                    .setMetadata(
                                            FeedActionMetadata.newBuilder()
                                                    .setType(Type.OPEN_URL_INCOGNITO)
                                                    .setOpenUrlData(
                                                            OpenUrlData.newBuilder().setUrl(URL)))
                                    .build()))
                    .build();

    private static final LabelledFeedActionData NORMAL_OPEN_URL =
            LabelledFeedActionData.newBuilder()
                    .setLabel(OPEN_LABEL)
                    .setFeedActionPayload(FeedActionPayload.newBuilder().setExtension(
                            FeedAction.feedActionExtension,
                            FeedAction.newBuilder()
                                    .setMetadata(
                                            FeedActionMetadata.newBuilder()
                                                    .setType(Type.OPEN_URL)
                                                    .setOpenUrlData(
                                                            OpenUrlData.newBuilder().setUrl(URL)))
                                    .build()))
                    .build();
    private static final String SESSION_ID = "SESSION_ID";
    private static final String CONTENT_ID = "CONTENT_ID";
    private static final ContentMetadata CONTENT_METADATA = new ContentMetadata(URL, "title",
            /* timePublished= */ -1,
            /* imageUrl= */ null,
            /* publisher= */ null,
            /* faviconUrl= */ null,
            /* snippet=*/null);

    @Mock
    private ActionApi mActionApi;
    @Mock
    private ActionParser mActionParser;
    @Mock
    private ActionManager mActionManager;
    @Mock
    private BasicLoggingApi mBasicLoggingApi;
    @Mock
    private ClusterPendingDismissHelper mClusterPendingDismissHelper;
    @Mock
    private ViewElementActionHandler mViewElementActionHandler;
    @Mock
    private TooltipApi mTooltipApi;

    @Captor
    private ArgumentCaptor<Consumer<String>> mConsumerCaptor;
    private ContentLoggingData mContentLoggingData;
    private FakeContextMenuManager mContextMenuManager;
    private StreamActionApiImpl mStreamActionApi;
    private View mView;

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Before
    public void setup() {
        initMocks(this);

        Context context = Robolectric.buildActivity(Activity.class).get();
        mContentLoggingData = new StreamContentLoggingData(0,
                BasicLoggingMetadata.getDefaultInstance(), RepresentationData.getDefaultInstance(),
                /* availableOffline= */ false);
        mView = new View(context);
        mContextMenuManager = new FakeContextMenuManager();
        mStreamActionApi =
                new StreamActionApiImpl(mActionApi, mActionParser, mActionManager, mBasicLoggingApi,
                        ()
                                -> mContentLoggingData,
                        mContextMenuManager, SESSION_ID, mClusterPendingDismissHelper,
                        mViewElementActionHandler, CONTENT_ID, mTooltipApi);
    }

    @Test
    public void testCanDismiss() {
        assertThat(mStreamActionApi.canDismiss()).isTrue();
    }

    @Test
    public void testDismiss() {
        String contentId = "contentId";
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        mStreamActionApi.dismiss(
                contentId, streamDataOperations, UndoAction.getDefaultInstance(), ACTION_PAYLOAD);

        verify(mActionManager)
                .dismissLocal(ImmutableList.of(contentId), streamDataOperations, SESSION_ID);
        verify(mBasicLoggingApi).onContentDismissed(mContentLoggingData, /*wasCommitted =*/true);
    }

    @Test
    public void testDismiss_withSnackbar_onCommitted() {
        testCommittedDismissWithSnackbar(Type.DISMISS);
    }

    @Test
    public void testDismiss_withSnackbar_onReverted() {
        testRevertedDismissWithSnackbar(Type.DISMISS);
    }

    @Test
    public void testDismiss_noSnackbar() {
        testDismissNoSnackbar(Type.DISMISS);
    }

    @Test
    public void testDismissWithSnackbar_dismissLocal_onCommitted() {
        testCommittedDismissWithSnackbar(Type.DISMISS_LOCAL);
    }

    @Test
    public void testDismissWithSnackbar_dismissLocal_onReverted() {
        testRevertedDismissWithSnackbar(Type.DISMISS_LOCAL);
    }

    @Test
    public void testDismissNoSnackbar_dismissLocal_onReverted() {
        testDismissNoSnackbar(Type.DISMISS_LOCAL);
    }

    @Test
    public void testHandleNotInterestedInTopic_onCommitted() {
        testCommittedDismissWithSnackbar(Type.NOT_INTERESTED_IN);
    }

    @Test
    public void testHandleNotInterestedInTopic_onReverted() {
        testRevertedDismissWithSnackbar(Type.NOT_INTERESTED_IN);
    }

    @Test
    public void testHandleNotInterestedInTopic_noSnackbar() {
        testDismissNoSnackbar(Type.NOT_INTERESTED_IN);
    }

    @Test
    public void testHandleBlockContent() {
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        mStreamActionApi.handleBlockContent(streamDataOperations, ACTION_PAYLOAD);

        verify(mActionManager).dismiss(streamDataOperations, SESSION_ID);
        verify(mActionManager)
                .createAndUploadAction(
                        CONTENT_ID, ACTION_PAYLOAD, ActionManager.UploadActionType.MISC);
    }

    @Test
    public void testOnClientAction() {
        mStreamActionApi.onClientAction(ActionType.OPEN_URL);

        verify(mBasicLoggingApi).onClientAction(mContentLoggingData, ActionType.OPEN_URL);
    }

    @Test
    public void testOnElementClick_logsElementClicked() {
        mStreamActionApi.onElementClick(ElementType.INTEREST_HEADER.getNumber());

        verify(mBasicLoggingApi)
                .onVisualElementClicked(
                        mContentLoggingData, ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnElementClick_logsElementClicked_retrievesLoggingDataLazily() {
        mContentLoggingData = new StreamContentLoggingData(1,
                BasicLoggingMetadata.getDefaultInstance(), RepresentationData.getDefaultInstance(),
                /* availableOffline= */ true);

        mStreamActionApi.onElementClick(ElementType.INTEREST_HEADER.getNumber());

        // Should use the new ContentLoggingData defined, not the old one.
        verify(mBasicLoggingApi)
                .onVisualElementClicked(
                        mContentLoggingData, ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOnElementView() {
        mStreamActionApi.onElementView(ElementType.INTEREST_HEADER.getNumber());

        verify(mViewElementActionHandler).onElementView(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    public void testReportClickAction_withFeature() {
        String contentId = "contentId";
        mStreamActionApi.reportClickAction(contentId, ACTION_PAYLOAD);

        verify(mActionManager)
                .createAndUploadAction(
                        contentId, ACTION_PAYLOAD, ActionManager.UploadActionType.CLICK);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    public void testNoReportClickAction_withoutFeature() {
        mStreamActionApi.reportClickAction("contentId", ACTION_PAYLOAD);

        verify(mActionManager, never())
                .createAndUploadAction(anyString(), any(ActionPayload.class),
                        any(ActionManager.UploadActionType.class));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    public void testReportViewVisible_withFeature() {
        String contentId = "contentId";
        mStreamActionApi.reportViewVisible(mView, contentId, ACTION_PAYLOAD);

        verify(mActionManager).onViewVisible(mView, contentId, ACTION_PAYLOAD);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    public void testReportViewVisible_withoutFeature() {
        mStreamActionApi.reportViewHidden(mView, "contentId");

        verify(mActionManager, never())
                .onViewVisible(any(View.class), anyString(), any(ActionPayload.class));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    public void testReportViewHidden_withFeature() {
        String contentId = "contentId";
        mStreamActionApi.reportViewHidden(mView, contentId);

        verify(mActionManager).onViewHidden(mView, contentId);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.REPORT_FEED_USER_ACTIONS)
    public void testReportViewHidden_withoutFeature() {
        mStreamActionApi.reportViewHidden(mView, "contentId");

        verify(mActionManager, never()).onViewHidden(any(View.class), anyString());
    }

    @Test
    public void testOnElementHide() {
        mStreamActionApi.onElementHide(ElementType.INTEREST_HEADER.getNumber());

        verify(mViewElementActionHandler).onElementHide(ElementType.INTEREST_HEADER.getNumber());
    }

    @Test
    public void testOpenUrl() {
        mStreamActionApi.openUrlInNewWindow(URL);
        verify(mActionApi).openUrlInNewWindow(URL);
    }

    @Test
    public void testOpenUrl_withParam() {
        mStreamActionApi.openUrl(URL, PARAM);

        verify(mActionManager)
                .uploadAllActionsAndUpdateUrl(eq(URL), eq(PARAM), mConsumerCaptor.capture());
        mConsumerCaptor.getValue().accept(NEW_URL);
        verify(mActionApi).openUrl(NEW_URL);
    }

    @Test
    public void testCanOpenUrl() {
        when(mActionApi.canOpenUrl()).thenReturn(true);
        assertThat(mStreamActionApi.canOpenUrl()).isTrue();

        when(mActionApi.canOpenUrl()).thenReturn(false);
        assertThat(mStreamActionApi.canOpenUrl()).isFalse();
    }

    @Test
    public void testCanOpenUrlInIncognitoMode() {
        when(mActionApi.canOpenUrlInIncognitoMode()).thenReturn(true);
        assertThat(mStreamActionApi.canOpenUrlInIncognitoMode()).isTrue();

        when(mActionApi.canOpenUrlInIncognitoMode()).thenReturn(false);
        assertThat(mStreamActionApi.canOpenUrlInIncognitoMode()).isFalse();
    }

    @Test
    public void testOpenUrlInIncognitoMode_withParam() {
        mStreamActionApi.openUrlInIncognitoMode(URL, PARAM);

        verify(mActionManager)
                .uploadAllActionsAndUpdateUrl(eq(URL), eq(PARAM), mConsumerCaptor.capture());
        mConsumerCaptor.getValue().accept(NEW_URL);
        verify(mActionApi).openUrlInIncognitoMode(NEW_URL);
    }

    @Test
    public void testOpenUrlInNewTab() {
        mStreamActionApi.openUrlInNewTab(URL);
        verify(mActionApi).openUrlInNewTab(URL);
    }

    @Test
    public void testOpenUrlInNewTab_withParam() {
        mStreamActionApi.openUrlInNewTab(URL, PARAM);

        verify(mActionManager)
                .uploadAllActionsAndUpdateUrl(eq(URL), eq(PARAM), mConsumerCaptor.capture());
        mConsumerCaptor.getValue().accept(NEW_URL);
        verify(mActionApi).openUrlInNewTab(NEW_URL);
    }

    @Test
    public void testCanOpenUrlInNewTab() {
        when(mActionApi.canOpenUrlInNewTab()).thenReturn(true);
        assertThat(mStreamActionApi.canOpenUrlInNewTab()).isTrue();

        when(mActionApi.canOpenUrlInNewTab()).thenReturn(false);
        assertThat(mStreamActionApi.canOpenUrlInNewTab()).isFalse();
    }

    @Test
    public void testDownloadUrl() {
        mStreamActionApi.downloadUrl(CONTENT_METADATA);
        verify(mActionApi).downloadUrl(CONTENT_METADATA);
    }

    @Test
    public void testCanDownloadUrl() {
        when(mActionApi.canDownloadUrl()).thenReturn(true);
        assertThat(mStreamActionApi.canDownloadUrl()).isTrue();

        when(mActionApi.canDownloadUrl()).thenReturn(false);
        assertThat(mStreamActionApi.canDownloadUrl()).isFalse();
    }

    @Test
    public void testLearnMore() {
        mStreamActionApi.learnMore();
        verify(mActionApi).learnMore();
    }

    @Test
    public void testCanLearnMore() {
        when(mActionApi.canLearnMore()).thenReturn(true);
        assertThat(mStreamActionApi.canLearnMore()).isTrue();

        when(mActionApi.canLearnMore()).thenReturn(false);
        assertThat(mStreamActionApi.canLearnMore()).isFalse();
    }

    @Test
    public void openContextMenuTest() {
        when(mActionParser.canPerformAction(any(FeedActionPayload.class), eq(mStreamActionApi)))
                .thenReturn(true);

        List<LabelledFeedActionData> labelledFeedActionDataList = new ArrayList<>();
        labelledFeedActionDataList.add(NORMAL_OPEN_URL);
        labelledFeedActionDataList.add(OPEN_IN_NEW_WINDOW);
        labelledFeedActionDataList.add(OPEN_IN_INCOGNITO_MODE);

        mStreamActionApi.openContextMenu(OpenContextMenuData.newBuilder()
                                                 .addAllContextMenuData(labelledFeedActionDataList)
                                                 .build(),
                mView);

        mContextMenuManager.performClick(0);
        mContextMenuManager.performClick(1);
        mContextMenuManager.performClick(2);

        InOrder inOrder = Mockito.inOrder(mActionParser);

        inOrder.verify(mActionParser)
                .parseFeedActionPayload(NORMAL_OPEN_URL.getFeedActionPayload(), mStreamActionApi,
                        mView, ActionSource.CONTEXT_MENU);
        inOrder.verify(mActionParser)
                .parseFeedActionPayload(OPEN_IN_NEW_WINDOW.getFeedActionPayload(), mStreamActionApi,
                        mView, ActionSource.CONTEXT_MENU);
        inOrder.verify(mActionParser)
                .parseFeedActionPayload(OPEN_IN_INCOGNITO_MODE.getFeedActionPayload(),
                        mStreamActionApi, mView, ActionSource.CONTEXT_MENU);
    }

    @Test
    public void openContextMenuTest_noNewWindow() {
        when(mActionParser.canPerformAction(
                     NORMAL_OPEN_URL.getFeedActionPayload(), mStreamActionApi))
                .thenReturn(true);
        when(mActionParser.canPerformAction(
                     OPEN_IN_INCOGNITO_MODE.getFeedActionPayload(), mStreamActionApi))
                .thenReturn(true);
        when(mActionParser.canPerformAction(
                     OPEN_IN_NEW_WINDOW.getFeedActionPayload(), mStreamActionApi))
                .thenReturn(false);

        List<LabelledFeedActionData> labelledFeedActionDataList = new ArrayList<>();
        labelledFeedActionDataList.add(NORMAL_OPEN_URL);
        labelledFeedActionDataList.add(OPEN_IN_NEW_WINDOW);
        labelledFeedActionDataList.add(OPEN_IN_INCOGNITO_MODE);

        mStreamActionApi.openContextMenu(OpenContextMenuData.newBuilder()
                                                 .addAllContextMenuData(labelledFeedActionDataList)
                                                 .build(),
                mView);

        assertThat(mContextMenuManager.getMenuOptions())
                .isEqualTo(Arrays.asList(OPEN_LABEL, OPEN_IN_INCOGNITO_MODE_LABEL));
    }

    @Test
    public void openContextMenuTest_noIncognitoWindow() {
        when(mActionParser.canPerformAction(
                     NORMAL_OPEN_URL.getFeedActionPayload(), mStreamActionApi))
                .thenReturn(true);
        when(mActionParser.canPerformAction(
                     OPEN_IN_NEW_WINDOW.getFeedActionPayload(), mStreamActionApi))
                .thenReturn(true);
        when(mActionParser.canPerformAction(
                     OPEN_IN_INCOGNITO_MODE.getFeedActionPayload(), mStreamActionApi))
                .thenReturn(false);

        List<LabelledFeedActionData> labelledFeedActionDataList = new ArrayList<>();
        labelledFeedActionDataList.add(NORMAL_OPEN_URL);
        labelledFeedActionDataList.add(OPEN_IN_NEW_WINDOW);
        labelledFeedActionDataList.add(OPEN_IN_INCOGNITO_MODE);

        mStreamActionApi.openContextMenu(OpenContextMenuData.newBuilder()
                                                 .addAllContextMenuData(labelledFeedActionDataList)
                                                 .build(),
                mView);

        assertThat(mContextMenuManager.getMenuOptions())
                .isEqualTo(Arrays.asList(OPEN_LABEL, OPEN_IN_NEW_WINDOW_LABEL));
    }

    @Test
    public void openContextMenuTest_logsContentContextMenuOpened() {
        when(mActionParser.canPerformAction(any(FeedActionPayload.class), eq(mStreamActionApi)))
                .thenReturn(true);

        mStreamActionApi.openContextMenu(
                OpenContextMenuData.newBuilder()
                        .addAllContextMenuData(Collections.singletonList(NORMAL_OPEN_URL))
                        .build(),
                mView);

        // First context menu succeeds in opening and is logged.
        verify(mBasicLoggingApi).onContentContextMenuOpened(mContentLoggingData);

        reset(mBasicLoggingApi);

        mStreamActionApi.openContextMenu(
                OpenContextMenuData.newBuilder()
                        .addAllContextMenuData(Collections.singletonList(NORMAL_OPEN_URL))
                        .build(),
                mView);

        // Second context menu fails in opening and is not logged.
        verifyZeroInteractions(mBasicLoggingApi);
    }

    @Test
    public void testMaybeShowTooltip() {
        TooltipInfo info = mock(TooltipInfo.class);
        ArgumentCaptor<TooltipCallbackApi> callbackCaptor =
                ArgumentCaptor.forClass(TooltipCallbackApi.class);

        mStreamActionApi.maybeShowTooltip(info, mView);

        verify(mTooltipApi).maybeShowHelpUi(eq(info), eq(mView), callbackCaptor.capture());

        callbackCaptor.getValue().onShow();
        verify(mViewElementActionHandler).onElementView(ElementType.TOOLTIP.getNumber());

        callbackCaptor.getValue().onHide(TooltipDismissType.TIMEOUT);
        verify(mViewElementActionHandler).onElementHide(ElementType.TOOLTIP.getNumber());
    }

    private void testCommittedDismissWithSnackbar(Type actionType) {
        ArgumentCaptor<PendingDismissCallback> pendingDismissCallback =
                ArgumentCaptor.forClass(PendingDismissCallback.class);
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        UndoAction undoAction =
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build();
        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                mStreamActionApi.dismiss(
                        CONTENT_ID, streamDataOperations, undoAction, ACTION_PAYLOAD);
                break;
            case NOT_INTERESTED_IN:
                mStreamActionApi.handleNotInterestedIn(
                        streamDataOperations, undoAction, ACTION_PAYLOAD, INTEREST_TYPE);
                break;
            default:
                break;
        }

        verify(mClusterPendingDismissHelper)
                .triggerPendingDismissForCluster(eq(undoAction), pendingDismissCallback.capture());

        pendingDismissCallback.getValue().onDismissCommitted();
        switch (actionType) {
            case DISMISS:
                verify(mActionManager)
                        .dismissLocal(
                                ImmutableList.of(CONTENT_ID), streamDataOperations, SESSION_ID);
                verify(mBasicLoggingApi)
                        .onContentDismissed(mContentLoggingData, /*wasCommitted =*/true);
                verify(mActionManager)
                        .createAndUploadAction(
                                CONTENT_ID, ACTION_PAYLOAD, ActionManager.UploadActionType.MISC);
                break;
            case DISMISS_LOCAL:
                verify(mActionManager)
                        .dismissLocal(
                                ImmutableList.of(CONTENT_ID), streamDataOperations, SESSION_ID);
                verify(mBasicLoggingApi)
                        .onContentDismissed(mContentLoggingData, /*wasCommitted =*/true);
                break;
            case NOT_INTERESTED_IN:
                verify(mActionManager).dismiss(streamDataOperations, SESSION_ID);
                verify(mBasicLoggingApi)
                        .onNotInterestedIn(
                                INTEREST_TYPE, mContentLoggingData, /*wasCommitted =*/true);
                verify(mActionManager)
                        .createAndUploadAction(
                                CONTENT_ID, ACTION_PAYLOAD, ActionManager.UploadActionType.MISC);
                break;
            default:
                break;
        }
    }

    private void testRevertedDismissWithSnackbar(Type actionType) {
        ArgumentCaptor<PendingDismissCallback> pendingDismissCallback =
                ArgumentCaptor.forClass(PendingDismissCallback.class);
        String contentId = "contentId";
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        UndoAction undoAction =
                UndoAction.newBuilder().setConfirmationLabel("confirmation").build();
        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                mStreamActionApi.dismiss(
                        contentId, streamDataOperations, undoAction, ACTION_PAYLOAD);
                break;
            case NOT_INTERESTED_IN:
                mStreamActionApi.handleNotInterestedIn(
                        streamDataOperations, undoAction, ACTION_PAYLOAD, INTEREST_TYPE);
                break;
            default:
                break;
        }

        verify(mClusterPendingDismissHelper)
                .triggerPendingDismissForCluster(eq(undoAction), pendingDismissCallback.capture());

        pendingDismissCallback.getValue().onDismissReverted();

        verify(mActionManager, never())
                .dismissLocal(ImmutableList.of(contentId), streamDataOperations, SESSION_ID);
        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                verify(mBasicLoggingApi)
                        .onContentDismissed(mContentLoggingData, /*wasCommitted =*/false);
                break;
            case NOT_INTERESTED_IN:
                verify(mBasicLoggingApi)
                        .onNotInterestedIn(
                                INTEREST_TYPE, mContentLoggingData, /*wasCommitted =*/false);
                break;
            default:
                break;
        }
    }

    private void testDismissNoSnackbar(Type actionType) {
        String contentId = "contentId";
        List<StreamDataOperation> streamDataOperations =
                Collections.singletonList(StreamDataOperation.getDefaultInstance());
        UndoAction undoAction = UndoAction.getDefaultInstance();
        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                mStreamActionApi.dismiss(
                        contentId, streamDataOperations, undoAction, ACTION_PAYLOAD);
                break;
            case NOT_INTERESTED_IN:
                mStreamActionApi.handleNotInterestedIn(
                        streamDataOperations, undoAction, ACTION_PAYLOAD, INTEREST_TYPE);
                break;
            default:
                break;
        }

        verify(mClusterPendingDismissHelper, never()).triggerPendingDismissForCluster(any(), any());

        switch (actionType) {
            case DISMISS:
            case DISMISS_LOCAL:
                verify(mActionManager)
                        .dismissLocal(
                                ImmutableList.of(contentId), streamDataOperations, SESSION_ID);
                verify(mBasicLoggingApi)
                        .onContentDismissed(mContentLoggingData, /*wasCommitted =*/true);
                break;
            case NOT_INTERESTED_IN:
                verify(mActionManager).dismiss(streamDataOperations, SESSION_ID);
                verify(mBasicLoggingApi)
                        .onNotInterestedIn(
                                INTEREST_TYPE, mContentLoggingData, /*wasCommitted =*/true);
                break;
            default:
                break;
        }
    }
}
