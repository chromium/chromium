// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.widget.ImageView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.settings.FaviconLoader;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.FaviconViewUtils;
import org.chromium.components.favicon.LargeIconBridge;

/**
 * A custom preference for displaying an actor login permission. Displays the site favicon, name,
 * and username with a "Revoke" button.
 */
@NullMarked
public class ActorLoginPermissionPreference extends ChromeBasePreference {
    private final ActorLoginPermission mPermission;
    private final Runnable mOnRevokeClicked;

    /**
     * Constructs a new ActorLoginPermissionPreference.
     *
     * @param context The android context.
     * @param permission The actor login permission data model.
     * @param largeIconBridge The bridge used to fetch the site's favicon.
     * @param onRevokeClicked The callback to run when the revoke button is clicked.
     */
    public ActorLoginPermissionPreference(
            Context context,
            ActorLoginPermission permission,
            LargeIconBridge largeIconBridge,
            Runnable onRevokeClicked) {
        super(context);
        mPermission = assumeNonNull(permission);
        mOnRevokeClicked = assumeNonNull(onRevokeClicked);

        setTitle(mPermission.getSiteOrAppName());
        setSummary(mPermission.getUsername());
        setWidgetLayoutResource(R.layout.preference_widget_revoke);
        setSelectable(false);

        // TODO(https://crbug.com/500353055): add a loading screen if this loads in too long.
        FaviconLoader.loadFavicon(
                context, largeIconBridge, mPermission.getFaviconUrl(), this::setIcon);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ImageView iconView = assertNonNull((ImageView) holder.findViewById(android.R.id.icon));
        FaviconViewUtils.formatIconForFavicon(getContext().getResources(), iconView);

        ImageView revokeButton = assertNonNull((ImageView) holder.findViewById(R.id.revoke_icon));
        revokeButton.setContentDescription(
                getContext()
                        .getString(
                                R.string.settings_glic_revoke_actor_login_permission_aria_label,
                                mPermission.getSiteOrAppName()));
        revokeButton.setOnClickListener(v -> mOnRevokeClicked.run());
    }
}
