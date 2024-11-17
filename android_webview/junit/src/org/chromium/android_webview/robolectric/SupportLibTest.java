// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.util.Features;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;

/**
 * Tests to prevent removing features we should avoid removing from the AndroidX Support library.
 * androidx.webkit 1.0.0 assumes these features are present and calls the methods.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SupportLibTest {
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFeaturesExist() {
        List<String> supportedFeaturesList =
                Arrays.asList(SupportLibWebViewChromiumFactory.getSupportedFeaturesForTesting());
        Assert.assertTrue(
                supportedFeaturesList.contains(Features.SAFE_BROWSING_RESPONSE_SHOW_INTERSTITIAL));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMethodsExist() {
        testMethodExists(WebMessageBoundaryInterface.class, "getData", null, String.class);
        testMethodExists(
                WebMessageBoundaryInterface.class,
                "getMessagePayload",
                null,
                InvocationHandler.class);
        testMethodExists(
                WebMessageBoundaryInterface.class, "getPorts", null, InvocationHandler[].class);
    }

    private void testMethodExists(
            Class<?> clazz, String method, Class<?>[] parameters, Class<?> returnType) {
        try {
            Method methodToFind = clazz.getMethod(method, parameters);
            Class<?> foundReturnType = methodToFind.getReturnType();
            Assert.assertEquals(
                    String.format(
                            "Method %s has incorrect return type of %s", method, foundReturnType),
                    returnType,
                    foundReturnType);
        } catch (NoSuchMethodException | SecurityException e) {
            throw new AssertionError(
                    String.format("Method %s not found in class %s", method, clazz.getName()), e);
        }
    }
}
