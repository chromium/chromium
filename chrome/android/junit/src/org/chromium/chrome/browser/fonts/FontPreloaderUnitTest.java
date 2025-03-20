// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fonts;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Typeface;
import android.os.Handler;

import androidx.core.content.res.ResourcesCompat;
import androidx.core.content.res.ResourcesCompat.FontCallback;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.annotation.Resetter;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.fonts.FontPreloaderUnitTest.ShadowResourcesCompat;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link FontPreloader}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowResourcesCompat.class})
@LooperMode(Mode.PAUSED)
public class FontPreloaderUnitTest {
    private static final Integer[] FONTS = {
        org.chromium.chrome.R.font.chrome_google_sans,
        org.chromium.chrome.R.font.chrome_google_sans_medium,
        org.chromium.chrome.R.font.chrome_google_sans_bold
    };

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mContext;

    private FontPreloader mFontPreloader;

    @Implements(ResourcesCompat.class)
    static class ShadowResourcesCompat {
        private static final List<Integer> sFontsRequested = new ArrayList<>();
        private static FontCallback sFontCallback;

        @Resetter
        public static void reset() {
            sFontsRequested.clear();
            sFontCallback = null;
        }

        @Implementation
        public static void getFont(
                Context context, int id, FontCallback fontCallback, Handler handler) {
            sFontCallback = fontCallback;
            sFontsRequested.add(id);
        }

        public static void loadFont() {
            sFontCallback.onFontRetrieved(Typeface.DEFAULT);
        }
    }

    @Before
    public void setUp() {
        ShadowResourcesCompat.reset();
        when(mContext.getApplicationContext()).thenReturn(mContext);
        mFontPreloader = new FontPreloader(FONTS);
        mFontPreloader.load(mContext);
    }

    @Test
    public void testGetFontCalledForAllFontsInArray() {
        assertThat(ShadowResourcesCompat.sFontsRequested, containsInAnyOrder(FONTS));
    }
}
