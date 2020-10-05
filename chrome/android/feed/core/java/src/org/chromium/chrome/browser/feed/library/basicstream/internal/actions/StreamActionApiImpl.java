// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.actions;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;
import org.chromium.chrome.browser.feed.library.api.host.action.ActionApi;
import org.chromium.chrome.browser.feed.library.api.host.action.StreamActionApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ActionType;
import org.chromium.chrome.browser.feed.library.api.host.logging.BasicLoggingApi;
import org.chromium.chrome.browser.feed.library.api.host.logging.ContentLoggingData;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipCallbackApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipInfo;
import org.chromium.chrome.browser.feed.library.api.internal.actionmanager.ActionManager;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionParser;
import org.chromium.chrome.browser.feed.library.api.internal.actionparser.ActionSource;
import org.chromium.chrome.browser.feed.library.basicstream.internal.pendingdismiss.ClusterPendingDismissHelper;
import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager;
import org.chromium.chrome.browser.feed.library.sharedstream.pendingdismiss.PendingDismissCallback;
import org.chromium.chrome.browser.feed.shared.FeedFeatures;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamDataOperation;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.FeedActionMetadata.ElementType;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.LabelledFeedActionData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.OpenContextMenuData;
import org.chromium.components.feed.core.proto.ui.action.FeedActionProto.UndoAction;
import org.chromium.components.feed.core.proto.wire.ActionPayloadProto.ActionPayload;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Action handler for Stream. */
public class StreamActionApiImpl implements StreamActionApi {
    private static final String TAG = "StreamActionApiImpl";

    private final ActionApi mActionApi;
    private final ActionParser mActionParser;
    private final ActionManager mActionManager;
    private final BasicLoggingApi mBasicLoggingApi;
    private final Supplier<ContentLoggingData> mContentLoggingData;
    private final ContextMenuManager mContextMenuManager;
    private final ClusterPendingDismissHelper mClusterPendingDismissHelper;
    private final ViewElementActionHandler mViewElementActionHandler;
    private final String mContentId;
    private final TooltipApi mTooltipApi;

    @Nullable
    private final String mSessionId;

    public StreamActionApiImpl(ActionApi actionApi, ActionParser actionParser,
            ActionManager actionManager, BasicLoggingApi basicLoggingApi,
            Supplier<ContentLoggingData> contentLoggingData, ContextMenuManager contextMenuManager,
            @Nullable String sessionId, ClusterPendingDismissHelper clusterPendingDismissHelper,
            ViewElementActionHandler viewElementActionHandler, String contentId,
            TooltipApi tooltipApi) {
        this.mActionApi = actionApi;
        this.mActionParser = actionParser;
        this.mActionManager = actionManager;
        this.mBasicLoggingApi = basicLoggingApi;
        this.mContentLoggingData = contentLoggingData;
        this.mContextMenuManager = contextMenuManager;
        this.mSessionId = sessionId;
        this.mClusterPendingDismissHelper = clusterPendingDismissHelper;
        this.mViewElementActionHandler = viewElementActionHandler;
        this.mContentId = contentId;
        this.mTooltipApi = tooltipApi;
    }

    @Override
    public void openContextMenu(OpenContextMenuData openContextMenuData, View anchorView) {
        List<String> actionLabels = new ArrayList<>();
        List<LabelledFeedActionData> enabledActions = new ArrayList<>();
        for (LabelledFeedActionData labelledFeedActionData :
                openContextMenuData.getContextMenuDataList()) {
            if (mActionParser.canPerformAction(
                        labelledFeedActionData.getFeedActionPayload(), this)) {
                actionLabels.add(labelledFeedActionData.getLabel());
                enabledActions.add(labelledFeedActionData);
            }
        }

        boolean menuOpened = mContextMenuManager.openContextMenu(anchorView, actionLabels,
                (int position)
                        -> mActionParser.parseFeedActionPayload(
                                enabledActions.get(position).getFeedActionPayload(),
                                StreamActionApiImpl.this, anchorView, ActionSource.CONTEXT_MENU));

        if (menuOpened) {
            mBasicLoggingApi.onContentContextMenuOpened(mContentLoggingData.get());
        }
    }

    @Override
    public boolean canOpenContextMenu() {
        return true;
    }

    @Override
    public boolean canDismiss() {
        return true;
    }

    @Override
    public boolean canHandleNotInterestedIn() {
        return true;
    }

    @Override
    public void handleNotInterestedIn(List<StreamDataOperation> dataOperations,
            UndoAction undoAction, ActionPayload payload, int interestType) {
        if (!undoAction.hasConfirmationLabel()) {
            dismiss(dataOperations);
            mBasicLoggingApi.onNotInterestedIn(
                    interestType, mContentLoggingData.get(), /* wasCommitted = */ true);
        } else {
            dismissWithSnackbar(undoAction, new PendingDismissCallback() {
                @Override
                public void onDismissReverted() {
                    mBasicLoggingApi.onNotInterestedIn(
                            interestType, mContentLoggingData.get(), /* wasCommitted = */ false);
                }

                @Override
                public void onDismissCommitted() {
                    dismiss(dataOperations);
                    mActionManager.createAndUploadAction(
                            mContentId, payload, ActionManager.UploadActionType.MISC);
                    mBasicLoggingApi.onNotInterestedIn(
                            interestType, mContentLoggingData.get(), /* wasCommitted = */ true);
                }
            });
        }
    }

    @Override
    public void handleBlockContent(
            List<StreamDataOperation> dataOperations, ActionPayload payload) {
        dismiss(dataOperations);
        mActionManager.createAndUploadAction(
                mContentId, payload, ActionManager.UploadActionType.MISC);
    }

    @Override
    public void dismiss(String contentId, List<StreamDataOperation> dataOperations,
            UndoAction undoAction, ActionPayload payload) {
        if (!undoAction.hasConfirmationLabel()) {
            dismissLocal(contentId, dataOperations);
            mBasicLoggingApi.onContentDismissed(
                    mContentLoggingData.get(), /* wasCommitted = */ true);
        } else {
            dismissWithSnackbar(undoAction, new PendingDismissCallback() {
                @Override
                public void onDismissReverted() {
                    mBasicLoggingApi.onContentDismissed(
                            mContentLoggingData.get(), /* wasCommitted = */ false);
                }

                @Override
                public void onDismissCommitted() {
                    dismissLocal(contentId, dataOperations);
                    dismiss(dataOperations);
                    mActionManager.createAndUploadAction(
                            contentId, payload, ActionManager.UploadActionType.MISC);
                    mBasicLoggingApi.onContentDismissed(
                            mContentLoggingData.get(), /* wasCommitted = */ true);
                }
            });
        }
    }

    private void dismiss(List<StreamDataOperation> dataOperations) {
        mActionManager.dismiss(dataOperations, mSessionId);
    }

    private void dismissLocal(String contentId, List<StreamDataOperation> dataOperations) {
        mActionManager.dismissLocal(
                Collections.singletonList(contentId), dataOperations, mSessionId);
    }

    private void dismissWithSnackbar(
            UndoAction undoAction, PendingDismissCallback pendingDismissCallback) {
        mClusterPendingDismissHelper.triggerPendingDismissForCluster(
                undoAction, pendingDismissCallback);
    }

    @Override
    public void onClientAction(@ActionType int actionType) {
        mBasicLoggingApi.onClientAction(mContentLoggingData.get(), actionType);
    }

    @Override
    public void openUrl(String url) {
        mActionApi.openUrl(url);
    }

    @Override
    public void openUrl(String url, String consistencyTokenQueryParamName) {
        mActionManager.uploadAllActionsAndUpdateUrl(
                url, consistencyTokenQueryParamName, result -> { mActionApi.openUrl(result); });
    }

    @Override
    public boolean canOpenUrl() {
        return mActionApi.canOpenUrl();
    }

    @Override
    public void openUrlInIncognitoMode(String url) {
        mActionApi.openUrlInIncognitoMode(url);
    }

    @Override
    public void openUrlInIncognitoMode(String url, String consistencyTokenQueryParamName) {
        mActionManager.uploadAllActionsAndUpdateUrl(url, consistencyTokenQueryParamName,
                result -> { mActionApi.openUrlInIncognitoMode(result); });
    }

    @Override
    public boolean canOpenUrlInIncognitoMode() {
        return mActionApi.canOpenUrlInIncognitoMode();
    }

    @Override
    public void openUrlInNewTab(String url) {
        mActionApi.openUrlInNewTab(url);
    }

    @Override
    public void openUrlInNewTab(String url, String consistencyTokenQueryParamName) {
        mActionManager.uploadAllActionsAndUpdateUrl(url, consistencyTokenQueryParamName,
                result -> { mActionApi.openUrlInNewTab(result); });
    }

    @Override
    public boolean canOpenUrlInNewTab() {
        return mActionApi.canOpenUrlInNewTab();
    }

    @Override
    public void openUrlInNewWindow(String url) {
        mActionApi.openUrlInNewWindow(url);
    }

    @Override
    public void openUrlInNewWindow(String url, String consistencyTokenQueryParamName) {
        mActionManager.uploadAllActionsAndUpdateUrl(url, consistencyTokenQueryParamName,
                result -> { mActionApi.openUrlInNewWindow(result); });
    }

    @Override
    public boolean canOpenUrlInNewWindow() {
        return mActionApi.canOpenUrlInNewWindow();
    }

    @Override
    public void downloadUrl(ContentMetadata contentMetadata) {
        mActionApi.downloadUrl(contentMetadata);
    }

    @Override
    public boolean canDownloadUrl() {
        return mActionApi.canDownloadUrl();
    }

    @Override
    public void sendFeedback(ContentMetadata contentMetadata) {
        mActionApi.sendFeedback(contentMetadata);
    }

    @Override
    public void learnMore() {
        mActionApi.learnMore();
    }

    @Override
    public boolean canLearnMore() {
        return mActionApi.canLearnMore();
    }

    @Override
    public boolean canHandleElementView() {
        return true;
    }

    @Override
    public boolean canHandleElementHide() {
        return true;
    }

    @Override
    public boolean canHandleElementClick() {
        return true;
    }

    @Override
    public void onElementView(int elementType) {
        mViewElementActionHandler.onElementView(elementType);
    }

    @Override
    public void onElementHide(int elementType) {
        mViewElementActionHandler.onElementHide(elementType);
    }

    @Override
    public void onElementClick(int elementType) {
        mBasicLoggingApi.onVisualElementClicked(mContentLoggingData.get(), elementType);
    }

    @Override
    public boolean canShowTooltip() {
        return true;
    }

    @Override
    public void maybeShowTooltip(TooltipInfo tooltipInfo, View view) {
        mTooltipApi.maybeShowHelpUi(tooltipInfo, view, new TooltipCallbackApi() {
            @Override
            public void onShow() {
                onElementView(ElementType.TOOLTIP.getNumber());
            }

            @Override
            public void onHide(int dismissType) {
                onElementHide(ElementType.TOOLTIP.getNumber());
            }
        });
    }

    @Override
    public void reportClickAction(String contentId, ActionPayload payload) {
        if (FeedFeatures.isReportingUserActions()) {
            mActionManager.createAndUploadAction(
                    contentId, payload, ActionManager.UploadActionType.CLICK);
        }
    }

    @Override
    public void reportViewVisible(View view, String contentId, ActionPayload payload) {
        if (FeedFeatures.isReportingUserActions()) {
            mActionManager.onViewVisible(view, contentId, payload);
        }
    }

    @Override
    public void reportViewHidden(View view, String contentId) {
        if (FeedFeatures.isReportingUserActions()) {
            mActionManager.onViewHidden(view, contentId);
        }
    }
}
