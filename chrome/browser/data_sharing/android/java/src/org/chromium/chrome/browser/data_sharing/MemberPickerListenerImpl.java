// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.util.Log;

import org.chromium.base.Callback;
import org.chromium.build.BuildConfig;
import org.chromium.components.data_sharing.DataSharingUIDelegate;

import java.util.List;

public class MemberPickerListenerImpl implements DataSharingUIDelegate.MemberPickerListener {

    private final Callback<List<String>> mCallback;

    public MemberPickerListenerImpl(Callback<List<String>> callback) {
        mCallback = callback;
    }

    public Callback<List<String>> getCallback() {
        return mCallback;
    }

    @Override
    public void onSelectionDone(List<String> selectedMemberIds, List<String> emails) {
        int count = 0;
        if (BuildConfig.ENABLE_ASSERTS) {
            for (String selectedMember : selectedMemberIds) {
                StringBuilder sb = new StringBuilder("MemberPickerListenerImpl selectedMemberIds ");
                sb.append(count++);
                sb.append(" : ");
                sb.append(selectedMember);
                Log.d("Logging", sb.toString());
            }
        }
        // TODO(ritikagup) : Call group management APIs to create group.
        mCallback.onResult(emails);
    }
}
