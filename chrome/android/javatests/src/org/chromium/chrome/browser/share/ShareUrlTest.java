// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.lang.ref.WeakReference;

/** Tests sharing URLs in reader mode (DOM distiller) */
// TODO(crbug.com/40256418): Remove this test when share no longer depends on DOM distiller.
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ShareUrlTest {
    private static final String HTTP_URL = "http://www.google.com/";
    private static final String HTTPS_URL = "https://www.google.com/";

    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @BeforeClass
    public static void setupBeforeCLass() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        sActivityTestRule.launchActivity(null);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock WindowAndroid mWindow;

    @Before
    public void setup() {
        Activity activity = sActivityTestRule.getActivity();
        Mockito.doReturn(new WeakReference<>(activity)).when(mWindow).getActivity();
    }

    private void assertCorrectUrl(final String originalUrl, final String sharedUrl) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    ShareParams params =
                            new ShareParams.Builder(mWindow, "", sharedUrl).setText("").build();
                    Intent intent = ShareHelper.getShareIntent(params);
                    Assert.assertTrue(intent.hasExtra(Intent.EXTRA_TEXT));
                    String url = intent.getStringExtra(Intent.EXTRA_TEXT);
                    Assert.assertEquals(originalUrl, url);
                });
    }

    @Test
    @SmallTest
    public void testNormalUrl() {
        assertCorrectUrl(HTTP_URL, HTTP_URL);
        assertCorrectUrl(HTTPS_URL, HTTPS_URL);
    }

    @Test
    @SmallTest
    public void testDistilledUrl() {
        final String domDistillerScheme = "chrome-distiller";
        String distilledHttpUrl =
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(
                        domDistillerScheme, HTTP_URL, "Title");
        String distilledHttpsUrl =
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(
                        domDistillerScheme, HTTPS_URL, "Title");

        assertCorrectUrl(HTTP_URL, distilledHttpUrl);
        assertCorrectUrl(HTTPS_URL, distilledHttpsUrl);
    }
}
