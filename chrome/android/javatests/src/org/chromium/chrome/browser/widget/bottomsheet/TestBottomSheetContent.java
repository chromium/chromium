// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.bottomsheet;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** A simple sheet content to test with. This only displays two empty white views. */
public class TestBottomSheetContent implements BottomSheetContent {
    /** {@link CallbackHelper} to ensure the destroy method is called. */
    public final CallbackHelper destroyCallbackHelper = new CallbackHelper();

    /** Empty view that represents the toolbar. */
    private View mToolbarView;

    /** Empty view that represents the content. */
    private View mContentView;

    /** This content's priority. */
    private @ContentPriority int mPriority;

    /** Whether this content is browser specific. */
    private boolean mHasCustomLifecycle;

    /** The peek height of this content. */
    private int mPeekHeight;

    /** The half height of this content. */
    private float mHalfHeight;

    /** The full height of this content. */
    private float mFullHeight;

    /**
     * @param context A context to inflate views with.
     * @param priority The content's priority.
     * @param hasCustomLifecycle Whether the content is browser specific.
     */
    public TestBottomSheetContent(
            Context context, @ContentPriority int priority, boolean hasCustomLifecycle) {
        mPeekHeight = BottomSheetContent.HeightMode.DEFAULT;
        mHalfHeight = BottomSheetContent.HeightMode.DEFAULT;
        mFullHeight = BottomSheetContent.HeightMode.DEFAULT;
        mPriority = priority;
        mHasCustomLifecycle = hasCustomLifecycle;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mToolbarView = new View(context);
            ViewGroup.LayoutParams params =
                    new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 100);
            mToolbarView.setLayoutParams(params);
            mToolbarView.setBackground(new ColorDrawable(Color.WHITE));

            mContentView = new View(context);
            params = new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            mContentView.setLayoutParams(params);
            mToolbarView.setBackground(new ColorDrawable(Color.WHITE));
        });
    }

    /**
     * @param context A context to inflate views with.
     */
    public TestBottomSheetContent(Context context) {
        this(/*TestBottomSheetContent(*/ context, ContentPriority.LOW, false);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {
        destroyCallbackHelper.notifyCalled();
    }

    @Override
    public int getPriority() {
        return mPriority;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    public void setPeekHeight(int height) {
        mPeekHeight = height;
    }

    @Override
    public int getPeekHeight() {
        return mPeekHeight;
    }

    public void setHalfHeightRatio(float ratio) {
        mHalfHeight = ratio;
    }

    @Override
    public float getHalfHeightRatio() {
        return mHalfHeight;
    }

    public void setFullHeightRatio(float ratio) {
        mFullHeight = ratio;
    }

    @Override
    public float getFullHeightRatio() {
        return mFullHeight;
    }

    @Override
    public boolean hasCustomLifecycle() {
        return mHasCustomLifecycle;
    }

    @Override
    public boolean setContentSizeListener(@Nullable ContentSizeListener listener) {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return android.R.string.copy;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return android.R.string.copy;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return android.R.string.copy;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return android.R.string.copy;
    }
}
