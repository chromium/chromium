// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.webkit.GeolocationPermissions;
import android.webkit.PermissionRequest;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import com.android.webview.chromium.ApiImplementationLogger;
import com.android.webview.chromium.ApiImplementationLogger.WebChromeClientMethod;
import com.android.webview.chromium.ApiImplementationLogger.WebViewClientMethod;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.base.Log;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.HistogramWatcher;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/** Tests for the {@link com.android.webview.chromium.ApiImplementationLogger}. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class ApiImplementationLoggerTest extends AwParameterizedTest {

    private static final String TAG = "ApiImplTest";
    @Rule public AwActivityTestRule mActivityTestRule;

    public ApiImplementationLoggerTest(@NonNull AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @NonNull
    private List<Method> getOverridableMethods(@NonNull Class<?> clazz) {
        List<Method> overridable = new ArrayList<>();
        for (Method method : clazz.getDeclaredMethods()) {
            if ((method.getModifiers() & (Modifier.PUBLIC | Modifier.PROTECTED)) > 0) {
                overridable.add(method);
            }
        }
        return overridable;
    }

    private @NonNull String switchCaseLine(@NonNull Class<?> clazz, @NonNull Method method) {
        Class<?>[] parameterTypes = method.getParameterTypes();
        List<String> typeNames = new ArrayList<>();
        for (Class<?> paramType : parameterTypes) {
            typeNames.add(paramType.getSimpleName().toUpperCase(Locale.ROOT));
        }
        String enumName = method.getName().toUpperCase() + "_" + String.join("_", typeNames);
        return String.format(
                "case \"%s\": return %sMethod.%s;", method, clazz.getSimpleName(), enumName);
    }

    private @NonNull String enumEntryLine(@NonNull Method method) {
        Class<?>[] parameterTypes = method.getParameterTypes();
        List<String> typeNames = new ArrayList<>();
        for (Class<?> paramType : parameterTypes) {
            typeNames.add(paramType.getSimpleName());
        }
        return String.format(
                "<int value=\"\" label=\"%s %s(%s)\"/>",
                method.getReturnType().getSimpleName(),
                method.getName(),
                String.join(", ", typeNames));
    }

    @Test
    @SmallTest
    public void testWebViewClientMapping() {
        List<Method> overridableMethods = getOverridableMethods(WebViewClient.class);

        Set<Integer> mappings = new HashSet<>();
        for (Method method : overridableMethods) {
            @WebViewClientMethod
            int methodEnum = ApiImplementationLogger.toWebViewClientMethodEnum(method);
            mappings.add(methodEnum);
            if (methodEnum == WebViewClientMethod.UNKNOWN) {
                Log.i(
                        TAG,
                        "Missing mapping of method. Add \n\t%s\nand\n\t%s\nto the relevant files.",
                        switchCaseLine(WebViewClient.class, method),
                        enumEntryLine(method));
            }
        }

        Assert.assertFalse(
                "Methods are lacking correct mapping. Check logcat output for switch lines to add"
                        + " to mapping and enums.xml",
                mappings.contains(WebViewClientMethod.UNKNOWN));
        Assert.assertFalse(mappings.contains(WebViewClientMethod.COUNT));
        Assert.assertEquals(overridableMethods.size(), mappings.size());
    }

    @Test
    @SmallTest
    public void testWebChromeClientMapping() {
        List<Method> overridableMethods = getOverridableMethods(WebChromeClient.class);

        Set<Integer> mappings = new HashSet<>();
        for (Method method : overridableMethods) {
            @WebChromeClientMethod
            int methodEnum = ApiImplementationLogger.toWebChromeClientMethodEnum(method);
            mappings.add(methodEnum);
            if (methodEnum == WebChromeClientMethod.UNKNOWN) {
                Log.i(
                        TAG,
                        "Missing mapping of method. Add \n\t%s\nand\n\t%s\nto the relevant files.",
                        switchCaseLine(WebChromeClient.class, method),
                        enumEntryLine(method));
            }
        }

        Assert.assertFalse(
                "Methods are lacking correct mapping. Check logcat output for switch lines to add"
                        + " to mapping and enums.xml",
                mappings.contains(WebChromeClientMethod.UNKNOWN));
        Assert.assertFalse(mappings.contains(WebChromeClientMethod.COUNT));
        Assert.assertEquals(overridableMethods.size(), mappings.size());
    }

    @Test
    @SmallTest
    public void testWebViewClientMethodsRecorded() {
        WebViewClient client =
                new WebViewClient() {
                    @Nullable
                    @Override
                    public WebResourceResponse shouldInterceptRequest(WebView view, String url) {
                        return null;
                    }

                    @Nullable
                    @Override
                    public WebResourceResponse shouldInterceptRequest(
                            WebView view, WebResourceRequest request) {
                        return null;
                    }

                    @Override
                    public boolean shouldOverrideUrlLoading(
                            WebView view, WebResourceRequest request) {
                        return false;
                    }
                };

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.WebView.ApiCall.Overridden.WebViewClient.Count", 3)
                        .expectIntRecords(
                                "Android.WebView.ApiCall.Overridden.WebViewClient",
                                WebViewClientMethod.SHOULDINTERCEPTREQUEST_WEBVIEW_STRING,
                                WebViewClientMethod
                                        .SHOULDINTERCEPTREQUEST_WEBVIEW_WEBRESOURCEREQUEST,
                                WebViewClientMethod
                                        .SHOULDOVERRIDEURLLOADING_WEBVIEW_WEBRESOURCEREQUEST)
                        .build()) {

            ApiImplementationLogger.logWebViewClientImplementation(client);
            watcher.assertExpected();
        }
    }

    @Test
    @SmallTest
    public void testWebChromeClientMethodsRecorded() {
        WebChromeClient client =
                new WebChromeClient() {
                    @Override
                    public void onProgressChanged(WebView view, int newProgress) {}

                    @Override
                    public void onReceivedTitle(WebView view, String title) {}

                    @Override
                    public void onGeolocationPermissionsShowPrompt(
                            String origin, GeolocationPermissions.Callback callback) {}

                    @Override
                    public void onPermissionRequest(PermissionRequest request) {}
                };

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.WebView.ApiCall.Overridden.WebChromeClient.Count", 4)
                        .expectIntRecords(
                                "Android.WebView.ApiCall.Overridden.WebChromeClient",
                                WebChromeClientMethod.ONPROGRESSCHANGED_WEBVIEW_INT,
                                WebChromeClientMethod.ONRECEIVEDTITLE_WEBVIEW_STRING,
                                WebChromeClientMethod
                                        .ONGEOLOCATIONPERMISSIONSSHOWPROMPT_STRING_CALLBACK,
                                WebChromeClientMethod.ONPERMISSIONREQUEST_PERMISSIONREQUEST)
                        .build()) {

            ApiImplementationLogger.logWebChromeClientImplementation(client);
            watcher.assertExpected();
        }
    }

    @Test
    @SmallTest
    public void testWebViewClientNonApiMethodsNotRecorded() {
        WebViewClient client =
                new WebViewClient() {
                    public void onUnhandledInputEvent(WebView view, android.view.InputEvent event) {
                        // This method is removed in the API, but still exists in the base class.
                    }
                };

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.WebView.ApiCall.Overridden.WebViewClient.Count", 0)
                        .expectNoRecords("Android.WebView.ApiCall.Overridden.WebViewClient")
                        .build()) {

            ApiImplementationLogger.logWebViewClientImplementation(client);
            watcher.assertExpected();
        }
    }

    @Test
    @SmallTest
    public void testWebChromeClientNonApiMethodsNotRecorded() {
        WebChromeClient client =
                new WebChromeClient() {
                    public void onReachedMaxAppCacheSize(
                            long requiredStorage,
                            long quota,
                            android.webkit.WebStorage.QuotaUpdater quotaUpdater) {
                        // This method is removed in the API, but still exists in the base class.
                    }
                };

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.WebView.ApiCall.Overridden.WebChromeClient.Count", 0)
                        .expectNoRecords("Android.WebView.ApiCall.Overridden.WebChromeClient")
                        .build()) {

            ApiImplementationLogger.logWebChromeClientImplementation(client);
            watcher.assertExpected();
        }
    }
}
