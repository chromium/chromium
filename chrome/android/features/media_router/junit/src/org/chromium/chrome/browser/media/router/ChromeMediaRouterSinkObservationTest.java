// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.util.ArrayList;
import java.util.List;

/**
 * Sink observation tests for ChromeMediaRouter.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeMediaRouterSinkObservationTest extends ChromeMediaRouterTestBase {
    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceived() {
        mChromeMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        assertEquals(1, mChromeMediaRouter.getSinksPerSourcePerProviderForTest().size());
        assertEquals(
                1, mChromeMediaRouter.getSinksPerSourcePerProviderForTest().get(SOURCE_ID1).size());
        assertEquals(0,
                mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID1)
                        .get(mRouteProvider)
                        .size());
        assertEquals(1, mChromeMediaRouter.getSinksPerSourceForTest().size());
        assertEquals(0, mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size());
    }

    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceivedTwiceForOneSource() {
        mChromeMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        List<MediaSink> sinkList = new ArrayList<MediaSink>();
        MediaSink sink = new MediaSink(SINK_ID1, SINK_NAME1, null);
        sinkList.add(sink);
        mChromeMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, sinkList);

        assertEquals(1, mChromeMediaRouter.getSinksPerSourcePerProviderForTest().size());
        assertEquals(
                1, mChromeMediaRouter.getSinksPerSourcePerProviderForTest().get(SOURCE_ID1).size());
        assertEquals(1,
                mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID1)
                        .get(mRouteProvider)
                        .size());
        assertTrue(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                           .get(SOURCE_ID1)
                           .get(mRouteProvider)
                           .contains(sink));

        assertEquals(1, mChromeMediaRouter.getSinksPerSourceForTest().size());
        assertEquals(1, mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size());
        assertTrue(mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).contains(sink));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testRouterOnSinksReceivedForTwoSources() {
        mChromeMediaRouter.onSinksReceived(SOURCE_ID1, mRouteProvider, new ArrayList<MediaSink>());

        List<MediaSink> sinkList = new ArrayList<MediaSink>();
        MediaSink sink = new MediaSink(SINK_ID1, SINK_NAME1, null);
        sinkList.add(sink);
        mChromeMediaRouter.onSinksReceived(SOURCE_ID2, mRouteProvider, sinkList);

        assertEquals(2, mChromeMediaRouter.getSinksPerSourcePerProviderForTest().size());
        assertEquals(
                1, mChromeMediaRouter.getSinksPerSourcePerProviderForTest().get(SOURCE_ID1).size());
        assertEquals(0,
                mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID1)
                        .get(mRouteProvider)
                        .size());
        assertEquals(
                1, mChromeMediaRouter.getSinksPerSourcePerProviderForTest().get(SOURCE_ID2).size());
        assertEquals(1,
                mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                        .get(SOURCE_ID2)
                        .get(mRouteProvider)
                        .size());
        assertTrue(mChromeMediaRouter.getSinksPerSourcePerProviderForTest()
                           .get(SOURCE_ID2)
                           .get(mRouteProvider)
                           .contains(sink));
        assertEquals(2, mChromeMediaRouter.getSinksPerSourceForTest().size());
        assertEquals(0, mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID1).size());
        assertEquals(1, mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID2).size());
        assertTrue(mChromeMediaRouter.getSinksPerSourceForTest().get(SOURCE_ID2).contains(sink));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testNotLowRamDevice() {
        SysUtils.resetForTesting();
        CommandLine.getInstance().appendSwitch(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE);
        assertTrue(mChromeMediaRouter.startObservingMediaSinks(SOURCE_ID1));
    }

    @Test
    @Feature({"MediaRouter"})
    public void testIsLowRamDevice() {
        SysUtils.resetForTesting();
        CommandLine.getInstance().appendSwitch(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE);
        assertEquals(false, mChromeMediaRouter.startObservingMediaSinks(SOURCE_ID1));
    }
}
