// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.content.Context;
import android.graphics.Point;
import android.graphics.drawable.Drawable;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;

import java.util.Objects;

/**
 * Encapsulates info necessary to overlay a circular badge (e.g., child account icon) on top of a
 * user avatar.
 */
@NullMarked
public final class BadgeConfig {
    private final int mBadgeResId;
    private final Drawable mBadge;
    private final @Px int mBadgeSize;
    private final @Px int mBorderSize;
    private final Point mPosition;

    private BadgeConfig(
            @DrawableRes int badgeResId,
            Drawable badge,
            @Px int badgeSize,
            @Px int borderSize,
            Point position) {
        assert badgeResId != 0;

        mBadgeResId = badgeResId;
        mBadge = badge;
        mBadgeSize = badgeSize;
        mBorderSize = borderSize;
        mPosition = position;
    }

    /** Returns the badge drawable. */
    Drawable getBadge() {
        return mBadge;
    }

    /** Returns the badge size in pixels. */
    @Px
    int getBadgeSize() {
        return mBadgeSize;
    }

    /** Returns the border size in pixels. */
    @Px
    int getBorderSize() {
        return mBorderSize;
    }

    /** Returns the position of the badge on the avatar in pixels. */
    Point getPosition() {
        return mPosition;
    }

    @Override
    public boolean equals(Object o) {
        return o instanceof BadgeConfig bc
                && mBadgeResId == bc.mBadgeResId
                && mBadgeSize == bc.mBadgeSize
                && mBorderSize == bc.mBorderSize
                && mPosition.equals(bc.mPosition);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mBadgeResId, mBadgeSize, mBorderSize, mPosition);
    }

    /** Returns a new {@link Builder} for the given badge drawable. */
    public static Builder create(@DrawableRes int badgeResId) {
        return new Builder(badgeResId);
    }

    /** Builder for {@link BadgeConfig}. */
    public static final class Builder {
        private final @DrawableRes int mBadgeResId;
        private @DimenRes int mBadgeSizeResId;
        private @DimenRes int mBorderSizeResId;
        private @DimenRes int mPositionXResId;
        private @DimenRes int mPositionYResId;

        private Builder(@DrawableRes int badgeResId) {
            assert badgeResId != 0;
            mBadgeResId = badgeResId;
        }

        /** Sets the full config for a child account badge. */
        public Builder withDefaultSizeChildAccountConfig() {
            return withBadgeSize(R.dimen.badge_size)
                    .withBorderSize(R.dimen.badge_border_size)
                    .withXPosition(R.dimen.badge_position_x)
                    .withYPosition(R.dimen.badge_position_y);
        }

        /* Set full config for toolbar identity disc. */
        public Builder withToolbarIdentityDiscConfig() {
            return withBadgeSize(R.dimen.toolbar_identity_disc_badge_size)
                    .withBorderSize(R.dimen.toolbar_identity_disc_badge_border_size)
                    .withXPosition(R.dimen.toolbar_identity_disc_badge_position_x)
                    .withYPosition(R.dimen.toolbar_identity_disc_badge_position_y);
        }

        /** Sets the badge size to the given dimension resource ID. */
        Builder withBadgeSize(@DimenRes int badgeSizeDimResId) {
            assert badgeSizeDimResId != 0;
            mBadgeSizeResId = badgeSizeDimResId;
            return this;
        }

        /** Sets the border size to the given dimension resource ID. */
        Builder withBorderSize(@DimenRes int borderSizeDimResId) {
            assert borderSizeDimResId != 0;
            mBorderSizeResId = borderSizeDimResId;
            return this;
        }

        /** Sets the X position to the given dimension resource ID. */
        Builder withXPosition(@DimenRes int positionXResId) {
            assert positionXResId != 0;
            mPositionXResId = positionXResId;
            return this;
        }

        /** Sets the Y position to the given dimension resource ID. */
        Builder withYPosition(@DimenRes int positionYResId) {
            assert positionYResId != 0;
            mPositionYResId = positionYResId;
            return this;
        }

        /** Builds a {@link BadgeConfig} with the given parameters. */
        public BadgeConfig build(Context context) {
            assert mBadgeSizeResId != 0 : "Badge size dimension resource ID must be set.";
            assert mBorderSizeResId != 0 : "Border size dimension resource ID must be set.";
            assert mPositionXResId != 0 : "Position X dimension resource ID must be set.";
            assert mPositionYResId != 0 : "Position Y dimension resource ID must be set.";

            var resources = context.getResources();
            return new BadgeConfig(
                    mBadgeResId,
                    AppCompatResources.getDrawable(context, mBadgeResId),
                    resources.getDimensionPixelSize(mBadgeSizeResId),
                    resources.getDimensionPixelSize(mBorderSizeResId),
                    new Point(
                            resources.getDimensionPixelOffset(mPositionXResId),
                            resources.getDimensionPixelOffset(mPositionYResId)));
        }
    }
}
