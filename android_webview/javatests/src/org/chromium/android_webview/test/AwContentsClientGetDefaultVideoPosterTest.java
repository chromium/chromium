// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Log;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.DefaultVideoPosterRequestHandler;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.TimeoutException;

/**
 * Tests for AwContentClient.GetDefaultVideoPoster.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwContentsClientGetDefaultVideoPosterTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

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
                    mPoster = BitmapFactory.decodeStream(
                            mContext.getAssets().open("asset_icon.png"));
                } catch (IOException e) {
                    Log.e(TAG, null, e);
                }
            }
            return mPoster;
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGetDefaultVideoPoster() throws Throwable {
        DefaultVideoPosterClient contentsClient = new DefaultVideoPosterClient(
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
    public void testInterceptDefaultVidoePosterURL() {
        DefaultVideoPosterClient contentsClient = new DefaultVideoPosterClient(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        DefaultVideoPosterRequestHandler handler =
                new DefaultVideoPosterRequestHandler(contentsClient);
        AwWebResourceResponse requestData =
                handler.shouldInterceptRequest(handler.getDefaultVideoPosterURL());
        Assert.assertTrue(requestData.getMimeType().equals("image/png"));
        Bitmap bitmap = BitmapFactory.decodeStream(requestData.getData());
        Bitmap poster = contentsClient.getPoster();
        Assert.assertEquals("poster.getHeight() not equal to bitmap.getHeight()",
                poster.getHeight(), bitmap.getHeight());
        Assert.assertEquals("poster.getWidth() not equal to bitmap.getWidth()", poster.getWidth(),
                bitmap.getWidth());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testNoDefaultVideoPoster() throws Throwable {
        NullContentsClient contentsClient = new NullContentsClient();
        DefaultVideoPosterRequestHandler handler =
                new DefaultVideoPosterRequestHandler(contentsClient);
        AwWebResourceResponse requestData =
                handler.shouldInterceptRequest(handler.getDefaultVideoPosterURL());
        Assert.assertTrue(requestData.getMimeType().equals("image/png"));
        InputStream in = requestData.getData();
        Assert.assertEquals("Should get -1", in.read(), -1);
    }
}
