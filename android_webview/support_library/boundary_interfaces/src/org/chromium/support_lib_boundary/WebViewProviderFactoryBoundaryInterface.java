// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.WebView;

import androidx.annotation.IntDef;

import org.jspecify.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.reflect.InvocationHandler;
import java.util.function.BiConsumer;
import java.util.function.Consumer;

/** Boundary interface for WebView globals and singletons. */
@NullMarked
public interface WebViewProviderFactoryBoundaryInterface {

    // LINT.IfChange(MultiCookieKeys)
    String MULTI_COOKIE_HEADER_NAME = "\0Set-Cookie-Multivalue\0";
    String MULTI_COOKIE_VALUE_SEPARATOR = "\0";

    // LINT.ThenChange(/components/embedder_support/android/util/web_resource_response.cc:MultiCookieKeys)

    /* WebViewBuilderBoundaryInterface */ InvocationHandler getWebViewBuilder();

    /* SupportLibraryWebViewChromium */ InvocationHandler createWebView(WebView webview);

    /* SupportLibWebkitToCompatConverter */ InvocationHandler getWebkitToCompatConverter();

    /* StaticsAdapter */ InvocationHandler getStatics();

    String[] getSupportedFeatures();

    /* SupportLibraryServiceWorkerController */ InvocationHandler getServiceWorkerController();

    /* SupportLibraryTracingController */ InvocationHandler getTracingController();

    /* SupportLibraryProxyController */ InvocationHandler getProxyController();

    /* DropDataContentProviderBoundaryInterface*/ InvocationHandler getDropDataProvider();

    /* ProfileStoreBoundaryInterface */ InvocationHandler getProfileStore();

    /** Use negative number for the features that are mandatory and shouldn't be ignored */
    @Target(ElementType.TYPE_USE)
    @IntDef({
        StartUpConfigField.BACKGROUND_EXECUTOR,
        StartUpConfigField.UI_THREAD_START_UP_TASKS,
        StartUpConfigField.PROFILE_NAMES_TO_LOAD,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface StartUpConfigField {
        int BACKGROUND_EXECUTOR = 0; /* Executor */
        int UI_THREAD_START_UP_TASKS = 1; /* Boolean */
        int PROFILE_NAMES_TO_LOAD = 2; /* Set<String> */
    }

    @Target(ElementType.TYPE_USE)
    @IntDef({
        StartUpResultField.TOTAL_TIME_UI_THREAD_MILLIS,
        StartUpResultField.MAX_TIME_PER_TASK_UI_THREAD_MILLIS,
        StartUpResultField.BLOCKING_START_UP_LOCATION,
        StartUpResultField.ASYNC_START_UP_LOCATION,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface StartUpResultField {
        int TOTAL_TIME_UI_THREAD_MILLIS = 0; /* Long */
        int MAX_TIME_PER_TASK_UI_THREAD_MILLIS = 1; /* Long */
        int BLOCKING_START_UP_LOCATION = 2; /* Throwable */
        int ASYNC_START_UP_LOCATION = 3; /* Throwable */
    }

    @Target(ElementType.TYPE_USE)
    @IntDef({
        StartupErrorType.CODE,
        StartupErrorType.MESSAGE,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface StartupErrorType {
        int CODE = 0; /* Integer */
        int MESSAGE = 1; /* String */
    }

    /**
     * Initial version of the API, covered by {@link
     * org.chromium.support_lib_boundary.util.Features.ASYNC_WEBVIEW_STARTUP_V2}
     */
    void startUpWebView(
            Consumer<BiConsumer<@StartUpConfigField Integer, Object>> config,
            Consumer<Consumer<BiConsumer<@StartUpResultField Integer, Object>>> onSuccess,
            Consumer<Consumer<BiConsumer<@StartupErrorType Integer, Object>>> onFailure);

    /**
     * Initial version of the API, covered by {@link
     * org.chromium.support_lib_boundary.util.Features.ASYNC_WEBVIEW_STARTUP}
     */
    void startUpWebView(
            /* WebViewStartUpConfig */ InvocationHandler config,
            /* WebViewStartUpCallback */ InvocationHandler callback);
}
