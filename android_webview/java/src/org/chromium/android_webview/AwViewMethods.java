// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.os.Bundle;
import android.view.DragEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeProvider;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

/**
 * An interface that defines a subset of the {@link View} functionality.
 *
 * <p>This interface allows us to hook up drawing and input related methods to the
 * {@link AwContents}'s consumer in embedded mode, and to the {@link FullScreenView}
 * in fullscreen mode.
 */
interface AwViewMethods {

    /** @see android.view.View#onDraw */
    void onDraw(Canvas canvas);

    /** @see android.view.View#onMeasure */
    void onMeasure(int widthMeasureSpec, int heightMeasureSpec);

    /** @see android.view.View#requestFocus */
    void requestFocus();

    /** @see android.view.View#setLayerType */
    void setLayerType(int layerType, Paint paint);

    /** @see android.view.View#onCreateInputConnection */
    InputConnection onCreateInputConnection(EditorInfo outAttrs);

    /** @see android.view.View#onDragEvent */
    boolean onDragEvent(DragEvent event);

    /** @see android.view.View#onKeyUp */
    boolean onKeyUp(int keyCode, KeyEvent event);

    /** @see android.view.View#dispatchKeyEvent */
    boolean dispatchKeyEvent(KeyEvent event);

    /** @see android.view.View#onTouchEvent */
    boolean onTouchEvent(MotionEvent event);

    /** @see android.view.View#onHoverEvent */
    boolean onHoverEvent(MotionEvent event);

    /** @see android.view.View#onGenericMotionEvent */
    boolean onGenericMotionEvent(MotionEvent event);

    /** @see android.view.View#onConfigurationChanged */
    void onConfigurationChanged(Configuration newConfig);

    /** @see android.view.View#onAttachedToWindow */
    void onAttachedToWindow();

    /** @see android.view.View#onDetachedFromWindow */
    void onDetachedFromWindow();

    /** @see android.view.View#onWindowFocusChanged */
    void onWindowFocusChanged(boolean hasWindowFocus);

    /** @see android.view.View#onFocusChanged */
    void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect);

    /** @see android.view.View#onSizeChanged */
    void onSizeChanged(int w, int h, int ow, int oh);

    /** @see android.view.View#onVisibilityChanged */
    void onVisibilityChanged(View changedView, int visibility);

    /** @see android.view.View#onWindowVisibilityChanged */
    void onWindowVisibilityChanged(int visibility);

    /** @see android.view.View#onScrollChanged */
    void onContainerViewScrollChanged(int l, int t, int oldl, int oldt);

    /** @see android.view.View#onOverScrolled */
    void onContainerViewOverScrolled(int scrollX, int scrollY, boolean clampedX, boolean clampedY);

    /** @see android.view.View#computeHorizontalScrollRange */
    int computeHorizontalScrollRange();

    /** @see android.view.View#computeHorizontalScrollOffset */
    int computeHorizontalScrollOffset();

    /** @see android.view.View#computeVerticalScrollRange */
    int computeVerticalScrollRange();

    /** @see android.view.View#computeVerticalScrollOffset */
    int computeVerticalScrollOffset();

    /** @see android.view.View#computeVerticalScrollExtent */
    int computeVerticalScrollExtent();

    /** @see android.view.View#computeScroll */
    void computeScroll();

    /** @see android.view.View#onCheckIsTextEditor */
    boolean onCheckIsTextEditor();

    /** @see android.view.View#getAccessibilityNodeProvider */
    AccessibilityNodeProvider getAccessibilityNodeProvider();

    /** @see android.view.View#performAccessibilityAction */
    public boolean performAccessibilityAction(final int action, final Bundle arguments);
}
