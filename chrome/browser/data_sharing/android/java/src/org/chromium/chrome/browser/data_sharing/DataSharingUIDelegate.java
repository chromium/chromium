// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.app.Activity;
import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.data_sharing.configs.AvatarConfig;
import org.chromium.chrome.browser.data_sharing.configs.GroupMemberConfig;
import org.chromium.chrome.browser.data_sharing.configs.MemberPickerConfig;

import java.util.List;

/** An interface that shows sharing UI screens. */
public interface DataSharingUIDelegate {

    /** Interface for callback when members are picked. */
    public interface MemberPickerListener {

        /** Called when members are selected. */
        public void onSelectionDone(List<String> selectedMemberIds);

        /** List of members picked. */
        public class MemberResult {
            // TODO (ritikagup) : Unify ids/avatar emails if needed.
            List<String> mSelectedMemberIds;
        }
    }

    /**
     * Displays UI to pick members for DataSharing UI.
     *
     * @param activity Used to associate with the given view group.
     * @param view The UI will be drawn inside the provided view.
     * @param memberResult This callback is invoked when members are selected.
     * @param config Used to set properties for the API.
     */
    public void showMemberPicker(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            MemberPickerListener memberResult,
            MemberPickerConfig config);

    /**
     * Displays FullPicker UI for DataSharing UI.
     *
     * @param activity Used to associate with the given view group.
     * @param view The UI will be drawn inside the provided view.
     * @param memberResult This callback is invoked when members are selected.
     * @param config Used to set properties for the API.
     */
    public void showFullPicker(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            MemberPickerListener memberResult,
            MemberPickerConfig config);

    /**
     * Method to display avatars in a single tile.
     *
     * @param context Used to associate with the given view group.
     * @param views The UI will be drawn inside the provided views.
     * @param emails Based on the list of emails, each view will be populated with avatars.
     * @param success Callback to tell if the avatars are fetched successfully.
     * @param config Used to set properties for the API.
     */
    public void showAvatars(
            @NonNull Context context,
            List<ViewGroup> views,
            List<String> emails,
            Callback<Boolean> success,
            AvatarConfig config);

    /**
     * Method to create group member list view.
     *
     * @param activity Used to associate with the given view group.
     * @param view The UI will be drawn inside the provided view.
     * @param groupId Used to associate the view with the group members.
     * @param tokenSecret Used to authenticate the user for whom the group member list is shown.
     * @param config Used to set properties for the API.
     */
    public void createGroupMemberListView(
            @NonNull Activity activity,
            @NonNull ViewGroup view,
            String groupId,
            String tokenSecret,
            GroupMemberConfig config);
}
