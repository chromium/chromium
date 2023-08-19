# Android WebView and the UI thread

This document describes how the Chromium based Android WebView reconciles with
mismatching requirements and assumptions between Android and Chromium about the
UI thread.


## The UI Thread in Chromium

In Chromium, there is a single global **UI** thread in the browser process. It
is one of the explicitly named
[BrowserThreads](/content/public/browser/browser_thread.h).
The **UI** thread is generally where UI code runs, including input delivery from
the OS.

The concept of “the one and only **UI** thread” is well established in content
layer and above. For example, all WebContents live on this **UI** thread.


## View Threads in Android

A [Window](https://developer.android.com/reference/android/view/Window.html),
its view tree, and
[Views](https://developer.android.com/reference/android/view/View) in that tree
are all single threaded in general; for a particular View, call this the
**view** thread. It is possible to create separate Windows and view trees on
separate threads, as long as Views are not moved between them. So there can be
more than one **view** thread.

Most apps do not make use of this hidden feature; they use a single view tree on
the [main thread](https://developer.android.com/reference/android/os/Looper#getMainLooper())
of the application. However, in general, it’s not safe to assume there’s only a
single **view** thread, or that the **view** thread and the **main** thread is
the same.


## Best effort solution

WebView’s solution is use the first **view** thread as the **UI** thread, then
post non-**UI** thread calls to the **UI** thread, and block if needed.

Because Chromium initialization identifies the thread as the UI thread, parts of
this solution runs before Chromium is initialized. The implementation is in the
android webview glue layer, specifically in
[WebViewChromium](../glue/java/src/com/android/webview/chromium/WebViewChromium.java)
and [WebViewChromiumFactoryProvider](../glue/java/src/com/android/webview/chromium/WebViewChromiumFactoryProvider.java).
The idea is hold off initializing chromium until a **view** thread is identified.

**View** thread can be identified by certain View methods. For example,
[onAttachedToWindow](https://developer.android.com/reference/android/view/View.html#onAttachedToWindow())
is called by the view tree on the **view** thread. Of course this is not 100%
reliable since apps could call those methods as well.

In case a method that cannot be held off is called before a view thread is
identified, the **main** thread is used as the **UI** thread. Methods calls that
are not held off include:

*   Methods with an immediate output with no meaningful defaults without initialization.
    *   onDraw: Need to draw into canvas parameter
    *   Any method with a return value, like getHttpAuthUsernamePassword.
*   “Important” calls that should not be held off. This is a judgement call mostly.
    *   Currently only loadUrl and loadData family of calls


## View is Single-Threaded

Views in android are expected to be created and used on its single **view**
thread, unless otherwise noted in the API. For WebView, this is enforced for
apps targeting JB MR2 or above. The code lives in
[WebView.java](https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/webkit/WebView.java).
Note this is an orthogonal concern to the single **UI** thread described above,
as this check still allows different WebViews to be used on different **view**
threads.
