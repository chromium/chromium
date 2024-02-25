// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.DefaultVideoPosterRequestHandler;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.TimeoutException;

/** Tests for AwContentClient.GetDefaultVideoPoster. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientGetDefaultVideoPosterTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String TAG = "AwContentsClientGetDefaultVideoPosterTest";

    private static class DefaultVideoPosterClient extends TestAwContentsClient {
        private CallbackHelper mVideoPosterCallbackHelper = new CallbackHelper();
        private Bitmap mPoster;
        private Context mContext;

        public DefaultVideoPosterClient(Context context) {
            mContext = context;
        }

        @Override
        public Bitmap getDefaultVideoPoster() {
            mVideoPosterCallbackHelper.notifyCalled();
            return getPoster();
        }

        public void waitForGetDefaultVideoPosterCalled() throws TimeoutException {
            mVideoPosterCallbackHelper.waitForCallback(0);
        }

        public Bitmap getPoster() {
            if (mPoster == null) {
                try {
                    mPoster =
                            BitmapFactory.decodeStream(mContext.getAssets().open("asset_icon.png"));
                } catch (IOException e) {
                    Log.e(TAG, null, e);
                }
            }
            return mPoster;
        }
    }

    public AwContentsClientGetDefaultVideoPosterTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testGetDefaultVideoPoster() throws Throwable {
        DefaultVideoPosterClient contentsClient =
                new DefaultVideoPosterClient(
                        InstrumentationRegistry.getInstrumentation().getContext());
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        String data = "<html><head><body><video id='video' control src='' /> </body></html>";
        mActivityTestRule.loadDataAsync(
                testContainerView.getAwContents(), data, "text/html", false);
        contentsClient.waitForGetDefaultVideoPosterCalled();
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    @SkipMutations(reason = "This test depends on AwSettings.setImagesEnabled(true)")
    public void testDefaultVideoPosterCSP() throws Throwable {
        DefaultVideoPosterClient contentsClient =
                new DefaultVideoPosterClient(
                        InstrumentationRegistry.getInstrumentation().getContext());
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        // Even though this content security policy does not allow loading from
        // android-webview-video-poster: this should still work as it's exempt from CSP.
        String data =
                "<html><head><meta http-equiv='Content-Security-Policy' content=\"default-src"
                        + " 'self';\"><body><video id='video' control src='' /> </body></html>";
        mActivityTestRule.loadDataAsync(
                testContainerView.getAwContents(), data, "text/html", false);
        contentsClient.waitForGetDefaultVideoPosterCalled();
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testInterceptDefaultVidoePosterURL() {
        DefaultVideoPosterClient contentsClient =
                new DefaultVideoPosterClient(
                        InstrumentationRegistry.getInstrumentation().getTargetContext());
        DefaultVideoPosterRequestHandler handler =
                new DefaultVideoPosterRequestHandler(contentsClient);
        WebResourceResponseInfo requestData =
                handler.shouldInterceptRequest(handler.getDefaultVideoPosterURL());
        Assert.assertTrue(requestData.getMimeType().equals("image/png"));
        Bitmap bitmap = BitmapFactory.decodeStream(requestData.getData());
        Bitmap poster = contentsClient.getPoster();
        Assert.assertEquals(
                "poster.getHeight() not equal to bitmap.getHeight()",
                poster.getHeight(),
                bitmap.getHeight());
        Assert.assertEquals(
                "poster.getWidth() not equal to bitmap.getWidth()",
                poster.getWidth(),
                bitmap.getWidth());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testNoDefaultVideoPoster() throws Throwable {
        NullContentsClient contentsClient = new NullContentsClient();
        DefaultVideoPosterRequestHandler handler =
                new DefaultVideoPosterRequestHandler(contentsClient);
        WebResourceResponseInfo requestData =
                handler.shouldInterceptRequest(handler.getDefaultVideoPosterURL());
        Assert.assertTrue(requestData.getMimeType().equals("image/png"));
        InputStream in = requestData.getData();
        Assert.assertEquals("Should get -1", in.read(), -1);
    }
}
