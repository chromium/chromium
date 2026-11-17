// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.everyItem;
import static org.hamcrest.Matchers.isIn;
import static org.hamcrest.Matchers.not;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Java unit tests for ChannelDefinitions. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeChannelDefinitionsTest {

    @Test
    public void testNoOverlapBetweenStartupAndLegacyChannelIds() {
        assertThat(
                ChromeChannelDefinitions.getInstance().getStartupChannelIds(),
                everyItem(not(isIn(ChromeChannelDefinitions.getInstance().getLegacyChannelIds()))));
    }

    @Test
    public void testReplacedLegacyChannelsAreLegacyChannels() {
        assertThat(
                ChromeChannelDefinitions.getInstance().getLegacyChannelReplacements().keySet(),
                everyItem(isIn(ChromeChannelDefinitions.getInstance().getLegacyChannelIds())));
    }

    @Test
    public void testLegacyChannelReplacementsArePredefined() {
        assertThat(
                ChromeChannelDefinitions.getInstance().getLegacyChannelReplacements().values(),
                everyItem(isIn(ChromeChannelDefinitions.getInstance().getAllChannelIds())));
    }

    @Test
    public void testLegacyChannelReplacementsAreNotStartupChannels() {
        // A startup channel is created before the legacy channels are cleaned up, so the legacy
        // channel would never be copied to it.
        assertThat(
                ChromeChannelDefinitions.getInstance().getLegacyChannelReplacements().values(),
                everyItem(
                        not(isIn(ChromeChannelDefinitions.getInstance().getStartupChannelIds()))));
    }
}
