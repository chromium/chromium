// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;

/**
 * Helper used to post the VIEW_SCROLLED accessibility event.
 *
 * TODO(mkosiba): Investigate whether this is behavior we want to share with the chrome/ layer.
 * TODO(mkosiba): We currently don't handle JS-initiated scrolling for layers other than the root
 * layer.
 */
class ScrollAccessibilityHelper {
    // This is copied straight out of android.view.ViewConfiguration.
    private static final long SEND_RECURRING_ACCESSIBILITY_EVENTS_INTERVAL_MILLIS = 100;

    private class HandlerCallback implements Handler.Callback {
        public static final int MSG_VIEW_SCROLLED = 1;

        private View mEventSender;

        public HandlerCallback(View eventSender) {
            mEventSender = eventSender;
        }

        @Override
        public boolean handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_VIEW_SCROLLED:
                    mMsgViewScrolledQueued = false;
                    mEventSender.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_SCROLLED);
                    break;
                default:
                    throw new IllegalStateException(
                            "AccessibilityInjector: unhandled message: " + msg.what);
            }
            return true;
        }
    }

    private Handler mHandler;
    private boolean mMsgViewScrolledQueued;
    private boolean mIsInAScroll;
    private boolean mEventSentByViewBaseClass;
    private final Runnable mSendRecurringViewScrolledEvents =
            new Runnable() {
                @Override
                public void run() {
                    if (!mEventSentByViewBaseClass) {
                        Message msg = mHandler.obtainMessage(HandlerCallback.MSG_VIEW_SCROLLED);
                        mHandler.sendMessage(msg);
                    } else {
                        mEventSentByViewBaseClass = false;
                    }
                    if (mIsInAScroll) {
                        mHandler.postDelayed(
                                this, SEND_RECURRING_ACCESSIBILITY_EVENTS_INTERVAL_MILLIS);
                    }
                }
            };

    public ScrollAccessibilityHelper(View eventSender) {
        mHandler = new Handler(new HandlerCallback(eventSender));
    }

    /**
     * Post a callback to send a {@link AccessibilityEvent#TYPE_VIEW_SCROLLED} event.
     * This event is sent at most once every
     * {@link android.view.ViewConfiguration#getSendRecurringAccessibilityEventsInterval()}
     */
    public void postViewScrolledAccessibilityEventCallback() {
        if (mMsgViewScrolledQueued) return;
        mMsgViewScrolledQueued = true;
        mEventSentByViewBaseClass = false;

        Message msg = mHandler.obtainMessage(HandlerCallback.MSG_VIEW_SCROLLED);
        mHandler.sendMessageDelayed(msg, SEND_RECURRING_ACCESSIBILITY_EVENTS_INTERVAL_MILLIS);
    }

    public void setIsInAScroll(boolean isScrolling) {
        mIsInAScroll = isScrolling;
        if (isScrolling) {
            mHandler.postDelayed(
                    mSendRecurringViewScrolledEvents,
                    SEND_RECURRING_ACCESSIBILITY_EVENTS_INTERVAL_MILLIS);
        }
    }

    public void removePostedViewScrolledAccessibilityEventCallback() {
        mEventSentByViewBaseClass = true;
        if (!mMsgViewScrolledQueued) return;
        mMsgViewScrolledQueued = false;

        mHandler.removeMessages(HandlerCallback.MSG_VIEW_SCROLLED);
    }

    public void removePostedCallbacks() {
        removePostedViewScrolledAccessibilityEventCallback();
        mHandler.removeCallbacks(mSendRecurringViewScrolledEvents);
    }
}
