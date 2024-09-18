// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.data_sharing.DataSharingNetworkLoader;
import org.chromium.components.data_sharing.DataSharingSDKDelegate;
import org.chromium.components.data_sharing.DataSharingSDKDelegateProtoResponseCallback;
import org.chromium.components.data_sharing.DataSharingSDKDelegateProtoResponseCallback.Status;
import org.chromium.components.data_sharing.protocol.AddAccessTokenParams;
import org.chromium.components.data_sharing.protocol.AddMemberParams;
import org.chromium.components.data_sharing.protocol.CreateGroupParams;
import org.chromium.components.data_sharing.protocol.DeleteGroupParams;
import org.chromium.components.data_sharing.protocol.LookupGaiaIdByEmailParams;
import org.chromium.components.data_sharing.protocol.ReadGroupsParams;
import org.chromium.components.data_sharing.protocol.RemoveMemberParams;

/** Implementation of {@link DataSharingSDKDelegate}. */
public class NoOpDataSharingSDKDelegateImpl implements DataSharingSDKDelegate {

    @Override
    public void initialize(DataSharingNetworkLoader networkLoader) {}

    @Override
    public void createGroup(
            CreateGroupParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.run(new byte[0], Status.FAILURE);
                });
    }

    @Override
    public void readGroups(
            ReadGroupsParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.run(new byte[0], Status.FAILURE);
                });
    }

    @Override
    public void addMember(AddMemberParams params, Callback<Integer> callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.onResult(Status.FAILURE);
                });
    }

    @Override
    public void removeMember(RemoveMemberParams params, Callback<Integer> callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.onResult(Status.FAILURE);
                });
    }

    @Override
    public void deleteGroup(DeleteGroupParams params, Callback<Integer> callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.onResult(Status.FAILURE);
                });
    }

    @Override
    public void lookupGaiaIdByEmail(
            LookupGaiaIdByEmailParams params,
            DataSharingSDKDelegateProtoResponseCallback callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.run(new byte[0], Status.FAILURE);
                });
    }

    @Override
    public void addAccessToken(
            AddAccessTokenParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    callback.run(new byte[0], Status.FAILURE);
                });
    }
}
