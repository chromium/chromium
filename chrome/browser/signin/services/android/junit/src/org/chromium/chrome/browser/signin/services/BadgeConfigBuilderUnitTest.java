// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BadgeConfig} and {@link BadgeConfig.Builder} */
@RunWith(BaseRobolectricTestRunner.class)
public class BadgeConfigBuilderUnitTest {

    @Test
    public void testDefaultSizeChildAccountBadgeConfig() {
        BadgeConfig childAccountConfig =
                BadgeConfig.create(R.drawable.ic_error_24dp)
                        .withDefaultSizeChildAccountConfig()
                        .build(RuntimeEnvironment.getApplication().getApplicationContext());

        BadgeConfig expectedConfig =
                BadgeConfig.create(R.drawable.ic_error_24dp)
                        .withBadgeSize(R.dimen.badge_size)
                        .withBorderSize(R.dimen.badge_border_size)
                        .withXPosition(R.dimen.badge_position_x)
                        .withYPosition(R.dimen.badge_position_y)
                        .build(RuntimeEnvironment.getApplication().getApplicationContext());

        Assert.assertEquals(expectedConfig, childAccountConfig);
    }

    @Test
    public void testToolbarIdentityDiscBadgeConfig() {
        var context = RuntimeEnvironment.getApplication().getApplicationContext();
        var resources = context.getResources();
        BadgeConfig identityDiscBadgeConfig =
                BadgeConfig.create(R.drawable.ic_error_badge_16dp)
                        .withToolbarIdentityDiscConfig()
                        .build(context);

        BadgeConfig expectedConfig =
                BadgeConfig.create(R.drawable.ic_error_badge_16dp)
                        .withBadgeSize(R.dimen.toolbar_identity_disc_badge_size)
                        .withBorderSize(R.dimen.toolbar_identity_disc_badge_border_size)
                        .withXPosition(R.dimen.toolbar_identity_disc_badge_position_x)
                        .withYPosition(R.dimen.toolbar_identity_disc_badge_position_y)
                        .build(RuntimeEnvironment.getApplication().getApplicationContext());

        Assert.assertEquals(expectedConfig, identityDiscBadgeConfig);
    }

    @Test
    public void testBadgeConfigBuilder() {
        var context = RuntimeEnvironment.getApplication().getApplicationContext();
        var resources = context.getResources();
        BadgeConfig badgeConfig =
                BadgeConfig.create(R.drawable.ic_error_badge_16dp)
                        .withBadgeSize(R.dimen.badge_size)
                        .withBorderSize(R.dimen.badge_border_size)
                        .withXPosition(R.dimen.badge_position_x)
                        .withYPosition(R.dimen.badge_position_y)
                        .build(context);

        Assert.assertEquals(
                resources.getDimensionPixelSize(R.dimen.badge_size), badgeConfig.getBadgeSize());
        Assert.assertEquals(
                resources.getDimensionPixelSize(R.dimen.badge_border_size),
                badgeConfig.getBorderSize());
        Assert.assertEquals(
                resources.getDimensionPixelOffset(R.dimen.badge_position_x),
                badgeConfig.getPosition().x);
        Assert.assertEquals(
                resources.getDimensionPixelOffset(R.dimen.badge_position_y),
                badgeConfig.getPosition().y);
        Assert.assertFalse(badgeConfig.keepsAiTierRing());
    }

    @Test
    public void testEditAvatarBadgeConfig() {
        var context = RuntimeEnvironment.getApplication().getApplicationContext();
        var resources = context.getResources();
        BadgeConfig editAvatarConfig =
                BadgeConfig.create(R.drawable.ic_error_24dp).withEditAvatarConfig().build(context);

        BadgeConfig expectedConfig =
                BadgeConfig.create(R.drawable.ic_error_24dp)
                        .withBadgeSize(R.dimen.edit_avatar_badge_size)
                        .withBorderSize(R.dimen.edit_avatar_badge_border_size)
                        .withXPosition(R.dimen.edit_avatar_badge_position_x)
                        .withYPosition(R.dimen.edit_avatar_badge_position_y)
                        .build(context);
        Assert.assertEquals(expectedConfig, editAvatarConfig);

        // The badge stays within the 60dp avatar it is designed for, so the badged image keeps
        // the avatar's size.
        int avatarSize = Math.round(60 * resources.getDisplayMetrics().density);
        Assert.assertTrue(
                editAvatarConfig.getPosition().x + editAvatarConfig.getBadgeSize() <= avatarSize);
        Assert.assertTrue(
                editAvatarConfig.getPosition().y + editAvatarConfig.getBadgeSize() <= avatarSize);
    }

    @Test
    public void testWithAiTierRingKeptTellsConfigsApart() {
        var context = RuntimeEnvironment.getApplication().getApplicationContext();
        BadgeConfig replacesRing =
                BadgeConfig.create(R.drawable.ic_error_24dp).withEditAvatarConfig().build(context);
        BadgeConfig keepsRing =
                BadgeConfig.create(R.drawable.ic_error_24dp)
                        .withEditAvatarConfig()
                        .withAiTierRingKept()
                        .build(context);

        Assert.assertFalse(replacesRing.keepsAiTierRing());
        Assert.assertTrue(keepsRing.keepsAiTierRing());
        // ProfileDataCache.setBadge() ignores a config equal to the current one.
        Assert.assertNotEquals(replacesRing, keepsRing);
        Assert.assertNotEquals(replacesRing.hashCode(), keepsRing.hashCode());
    }
}
