// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.TargetApi;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.lang.reflect.Method;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the SmartClipProvider.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SmartClipProviderTest implements Handler.Callback {
    // This is a key for meta-data in the package manifest. It should NOT
    // change, as OEMs will use it when they look for the SmartClipProvider
    // interface.

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String MOUNTAIN = "Mountain";

    private static final String DATA_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><meta name=\"viewport\""
            + "content=\"width=device-width, initial-scale=1.0, maximum-scale=1.0\" /></head>"
            + "<style type=\"text/css\"> #text {white-space:nowrap;}</style>"
            + "<title>" + MOUNTAIN + "</title>"
            + "<body><p><span id=\"simple_text\">" + MOUNTAIN + "</span></p>"
            + "</body></html>");

    private static final String SMART_CLIP_PROVIDER_KEY =
            "org.chromium.content.browser.SMART_CLIP_PROVIDER";

    private static class MyCallbackHelper extends CallbackHelper {
        public String getTitle() {
            return mTitle;
        }

        public String getUrl() {
            return mUrl;
        }

        public String getText() {
            return mText;
        }

        public String getHtml() {
            return mHtml;
        }

        public Rect getRect() {
            return mRect;
        }

        public void notifyCalled(String title, String url, String text, String html, Rect rect) {
            mTitle = title;
            mUrl = url;
            mText = text;
            mHtml = html;
            mRect = rect;
            super.notifyCalled();
        }

        private String mTitle;
        private String mUrl;
        private String mText;
        private String mHtml;
        private Rect mRect;
    }

    private ChromeActivity mActivity;
    private MyCallbackHelper mCallbackHelper;
    private HandlerThread mHandlerThread;
    private Handler mHandler;
    private Class<?> mSmartClipProviderClass;
    private Method mSetSmartClipResultHandlerMethod;
    private Method mExtractSmartClipDataMethod;
    private WebContents mWebContents;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityWithURL(DATA_URL);
        mActivity = mActivityTestRule.getActivity();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mWebContents = mActivityTestRule.getWebContents(); });

        DOMUtils.waitForNonZeroNodeBounds(mWebContents, "simple_text");

        mCallbackHelper = new MyCallbackHelper();
        mHandlerThread = new HandlerThread("ContentViewTest thread");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper(), this);

        mSmartClipProviderClass = getSmartClipProviderClass();
        Assert.assertNotNull(mSmartClipProviderClass);
        mSetSmartClipResultHandlerMethod = mSmartClipProviderClass.getDeclaredMethod(
                "setSmartClipResultHandler", new Class[] { Handler.class });
        mExtractSmartClipDataMethod = mSmartClipProviderClass.getDeclaredMethod(
                "extractSmartClipData",
                new Class[] { Integer.TYPE, Integer.TYPE, Integer.TYPE, Integer.TYPE });
    }

    @After
    @TargetApi(Build.VERSION_CODES.JELLY_BEAN_MR2)
    public void tearDown() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            mHandlerThread.quitSafely();
        } else {
            mHandlerThread.quit();
        }
    }

    // Implements Handler.Callback
    @Override
    public boolean handleMessage(Message msg) {
        Bundle bundle = msg.getData();
        Assert.assertNotNull(bundle);
        String url = bundle.getString("url");
        String title = bundle.getString("title");
        String text = bundle.getString("text");
        String html = bundle.getString("html");
        Rect rect = bundle.getParcelable("rect");
        // We don't care about other values for now.
        mCallbackHelper.notifyCalled(title, url, text, html, rect);
        return true;
    }

    // Create SmartClipProvider interface from package meta-data.
    private Class<?> getSmartClipProviderClass() throws Exception {
        ApplicationInfo ai = mActivity.getPackageManager().getApplicationInfo(
                mActivity.getPackageName(), PackageManager.GET_META_DATA);
        Bundle bundle = ai.metaData;
        String className = bundle.getString(SMART_CLIP_PROVIDER_KEY);
        Assert.assertNotNull(className);
        return Class.forName(className);
    }

    // Returns the first smart clip provider under the root view using DFS.
    private Object findSmartClipProvider(View v) {
        if (mSmartClipProviderClass.isInstance(v)) {
            return v;
        } else if (v instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) v;
            int count = viewGroup.getChildCount();
            for (int i = 0; i < count; ++i) {
                View c = viewGroup.getChildAt(i);
                Object found = findSmartClipProvider(c);
                if (found != null) return found;
            }
        }
        return null;
    }

    // Disable test on tablet since it fails consistently on M tablet. See https://crbug.com/853816
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Test
    @MediumTest
    @Feature({"SmartClip"})
    @RetryOnFailure
    public void testSmartClipDataCallback() throws TimeoutException {
        final float dpi = Coordinates.createFor(mWebContents).getDeviceScaleFactor();
        final Rect bounds = DOMUtils.getNodeBounds(mWebContents, "simple_text");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // This emulates what OEM will be doing when they want to call
            // functions on SmartClipProvider through view hierarchy.

            Object scp = findSmartClipProvider(
                    mActivityTestRule.getActivity().findViewById(android.R.id.content));
            Assert.assertNotNull(scp);
            try {
                mSetSmartClipResultHandlerMethod.invoke(scp, mHandler);
                mExtractSmartClipDataMethod.invoke(scp, (int) (bounds.left * dpi),
                        (int) (bounds.right * dpi), (int) (bounds.width() * dpi),
                        (int) (bounds.height() * dpi));
            } catch (Exception e) {
                e.printStackTrace();
                Assert.fail();
            }
        });
        mCallbackHelper.waitForCallback(0, 1);  // call count: 0 --> 1
        Assert.assertEquals(MOUNTAIN, mCallbackHelper.getTitle());
        Assert.assertEquals(DATA_URL, mCallbackHelper.getUrl());
        Assert.assertNotNull(mCallbackHelper.getText());
        Assert.assertNotNull(mCallbackHelper.getHtml());
        Assert.assertTrue(!mCallbackHelper.getRect().isEmpty());
    }

    @Test
    @MediumTest
    @Feature({"SmartClip"})
    @RetryOnFailure
    public void testSmartClipNoHandlerDoesntCrash() throws TimeoutException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Object scp = findSmartClipProvider(
                    mActivityTestRule.getActivity().findViewById(android.R.id.content));
            Assert.assertNotNull(scp);
            try {
                // Galaxy Note 4 has a bug where it doesn't always set the handler first; in
                // that case, we shouldn't crash: http://crbug.com/710147
                mExtractSmartClipDataMethod.invoke(scp, 10, 20, 100, 70);

                // Add a wait for a valid callback to make sure we have time to
                // hit the crash from the above call if any.
                mSetSmartClipResultHandlerMethod.invoke(scp, mHandler);
                mExtractSmartClipDataMethod.invoke(scp, 10, 20, 100, 70);
            } catch (Exception e) {
                e.printStackTrace();
                Assert.fail();
            }
        });
        mCallbackHelper.waitForCallback(0, 1); // call count: 0 --> 1
    }
}
