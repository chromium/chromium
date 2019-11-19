// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.support.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests {@link GoBackDirectActionHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GoBackDirectActionHandlerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private Runnable mAction;

    @Mock
    private DirectActionReporter mReporter;

    @Test
    @SmallTest
    public void testGoBack() {
        DirectActionHandler handler = new GoBackDirectActionHandler(mAction);

        handler.reportAvailableDirectActions(mReporter);
        Mockito.verify(mReporter).addDirectAction("go_back");

        List<Bundle> responses = new ArrayList<>();
        assertTrue(handler.performDirectAction(
                "go_back", Bundle.EMPTY, (response) -> responses.add(response)));

        assertThat(responses, Matchers.hasSize(1));
        Mockito.verify(mAction).run();
    }
}
