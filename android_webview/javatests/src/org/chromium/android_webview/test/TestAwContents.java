// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;

import java.util.ArrayList;

/**
 * The AwContents for testing, it provides a way for test to get internal state
 * of AwContents
 */
public class TestAwContents extends AwContents {
    /**
     * The observer of render process gone events.
     */
    public interface RenderProcessGoneObserver {
        /**
         * Invoked when AwContents notified AwContentsClient about render
         * process gone
         */
        void onRenderProcessGoneNotifiedToAwContentsClient();

        /**
         * Invoked when AwContents has been destroyed.
         */
        void onAwContentsDestroyed();
    }

    private ArrayList<RenderProcessGoneObserver> mRenderProcessGoneObservers;
    private RenderProcessGoneHelper mRenderProcessGoneHelper;

    public TestAwContents(AwBrowserContext browserContext, ViewGroup containerView, Context context,
            InternalAccessDelegate internalAccessAdapter,
            NativeDrawFunctorFactory nativeDrawFunctorFactory, AwContentsClient contentsClient,
            AwSettings settings, DependencyFactory dependencyFactory) {
        super(browserContext, containerView, context, internalAccessAdapter,
                nativeDrawFunctorFactory, contentsClient, settings, dependencyFactory);

        mRenderProcessGoneHelper = new RenderProcessGoneHelper();
        mRenderProcessGoneObservers = new ArrayList<RenderProcessGoneObserver>();
        mRenderProcessGoneObservers.add(mRenderProcessGoneHelper);
    }

    public RenderProcessGoneHelper getRenderProcessGoneHelper() {
        return mRenderProcessGoneHelper;
    }

    @Override
    protected boolean onRenderProcessGone(int childProcessID, boolean crashed) {
        boolean ret = super.onRenderProcessGone(childProcessID, crashed);
        for (RenderProcessGoneObserver observer : mRenderProcessGoneObservers) {
            observer.onRenderProcessGoneNotifiedToAwContentsClient();
        }
        return ret;
    }

    @Override
    protected void onDestroyed() {
        super.onDestroyed();
        for (RenderProcessGoneObserver observer : mRenderProcessGoneObservers) {
            observer.onAwContentsDestroyed();
        }
    }
}
