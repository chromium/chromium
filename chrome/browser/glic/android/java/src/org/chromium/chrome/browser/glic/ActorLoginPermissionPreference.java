// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.widget.ImageView;

import androidx.core.content.ContextCompat;
import androidx.preference.PreferenceViewHolder;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.favicon.LargeIconBridge;

/**
 * A custom preference for displaying an actor login permission. Displays the site favicon, name,
 * and username with a "Revoke" button.
 */
@NullMarked
public class ActorLoginPermissionPreference extends ChromeBasePreference {
    private final ActorLoginPermission mPermission;
    private final LargeIconBridge mLargeIconBridge;
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
        mLargeIconBridge = assumeNonNull(largeIconBridge);
        mOnRevokeClicked = assumeNonNull(onRevokeClicked);

        setTitle(mPermission.getSiteOrAppName());
        setSummary(mPermission.getUsername());
        setWidgetLayoutResource(R.layout.preference_widget_revoke);
        setSelectable(false); // Match Desktop's non-clickable behavior for the row.

        // Fetch favicon.
        int iconSize =
                context.getResources().getDimensionPixelSize(R.dimen.default_favicon_min_size);
        mLargeIconBridge.getLargeIconForUrl(
                mPermission.getUrl(), iconSize, this::onFaviconAvailable);
    }

    private void onFaviconAvailable(
            @Nullable Bitmap icon,
            int fallbackColor,
            boolean isFallbackColorDefault,
            int iconType) {
        if (icon != null) {
            setIcon(new BitmapDrawable(getContext().getResources(), icon));
        } else {
            setIcon(ContextCompat.getDrawable(getContext(), R.drawable.ic_globe_24dp));
        }
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        ImageView revokeButton = assertNonNull((ImageView) holder.findViewById(R.id.revoke_icon));
        revokeButton.setOnClickListener(v -> mOnRevokeClicked.run());
    }
}
