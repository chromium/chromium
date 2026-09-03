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
}
