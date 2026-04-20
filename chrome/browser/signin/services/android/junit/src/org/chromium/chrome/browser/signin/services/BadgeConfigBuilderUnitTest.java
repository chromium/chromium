// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BadgeConfig} and {@link BadgeConfig.Builder} */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class BadgeConfigBuilderUnitTest {

    @Test
    public void testDefaultSizeChildAccountBadgeConfig() {
        BadgeConfig childAccountConfig =
                BadgeConfig.create(R.drawable.ic_error)
                        .withDefaultSizeChildAccountConfig()
                        .build(RuntimeEnvironment.application.getApplicationContext());

        BadgeConfig expectedConfig =
                BadgeConfig.create(R.drawable.ic_error)
                        .withBadgeSize(R.dimen.badge_size)
                        .withBorderSize(R.dimen.badge_border_size)
                        .withXPosition(R.dimen.badge_position_x)
                        .withYPosition(R.dimen.badge_position_y)
                        .build(RuntimeEnvironment.application.getApplicationContext());

        Assert.assertEquals(expectedConfig, childAccountConfig);
    }

    @Test
    public void testToolbarIdentityDiscBadgeConfig() {
        var context = RuntimeEnvironment.application.getApplicationContext();
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
                        .build(RuntimeEnvironment.application.getApplicationContext());

        Assert.assertEquals(expectedConfig, identityDiscBadgeConfig);
    }

    @Test
    public void testBadgeConfigBuilder() {
        var context = RuntimeEnvironment.application.getApplicationContext();
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
    }
}
