// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import org.chromium.base.Callback;
import org.chromium.components.data_sharing.DataSharingSDKDelegate;
import org.chromium.components.data_sharing.DataSharingSDKDelegateProtoResponseCallback;
import org.chromium.components.data_sharing.protocol.AddMemberParams;
import org.chromium.components.data_sharing.protocol.CreateGroupParams;
import org.chromium.components.data_sharing.protocol.CreateGroupResult;
import org.chromium.components.data_sharing.protocol.DeleteGroupParams;
import org.chromium.components.data_sharing.protocol.LookupGaiaIdByEmailParams;
import org.chromium.components.data_sharing.protocol.LookupGaiaIdByEmailResult;
import org.chromium.components.data_sharing.protocol.ReadGroupsParams;
import org.chromium.components.data_sharing.protocol.ReadGroupsResult;
import org.chromium.components.data_sharing.protocol.RemoveMemberParams;

/**
 * Implementation of {@link DataSharingSDKDelegate}. The callbacks are invoked synchronously (in the
 * same stack frame) and is therefore re-entrant.
 */
public class DataSharingSDKDelegateImpl implements DataSharingSDKDelegate {

    @Override
    public void createGroup(
            CreateGroupParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        callback.run(CreateGroupResult.newBuilder().build().toByteArray(), /* status= */ 1);
    }

    @Override
    public void readGroups(
            ReadGroupsParams params, DataSharingSDKDelegateProtoResponseCallback callback) {
        callback.run(ReadGroupsResult.newBuilder().build().toByteArray(), /* status= */ 1);
    }

    @Override
    public void addMember(AddMemberParams params, Callback<Integer> callback) {
        callback.onResult(/* status= */ 1);
    }

    @Override
    public void removeMember(RemoveMemberParams params, Callback<Integer> callback) {
        callback.onResult(/* status= */ 1);
    }

    @Override
    public void deleteGroup(DeleteGroupParams params, Callback<Integer> callback) {
        callback.onResult(/* status= */ 1);
    }

    @Override
    public void lookupGaiaIdByEmail(
            LookupGaiaIdByEmailParams params,
            DataSharingSDKDelegateProtoResponseCallback callback) {
        callback.run(LookupGaiaIdByEmailResult.newBuilder().build().toByteArray(), /* status= */ 1);
    }
}
