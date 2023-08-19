// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import android.view.View.OnClickListener;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/** Tests for {@link SimpleHandleStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.CCT_RESIZABLE_FOR_THIRD_PARTIES,
        ChromeFeatureList.CCT_RESIZABLE_ALLOW_RESIZE_BY_USER_GESTURE})
@LooperMode(Mode.PAUSED)
public class SimpleHandleStrategyTest {
    @Test
    public void checkCloseClickHandlerInvocationAfterAnimation() {
        Callback<Runnable> animation = r -> r.run();
        OnClickListener closeClickListener = Mockito.mock(OnClickListener.class);

        var strategy = new SimpleHandleStrategy(animation);
        strategy.setCloseClickHandler(closeClickListener);
        strategy.startCloseAnimation();

        verify(closeClickListener).onClick(any());
    }
}
