## Android WebView & Insets


## Context

WebView is an Android component which you can use to display web content in your
app. Just like a web browser, WebView contains multiple
[viewports](https://developer.mozilla.org/en-US/docs/Glossary/Viewport) and your
content’s alignment may change based on the size of different viewports.

The
[Layout Viewport](https://developer.mozilla.org/en-US/docs/Glossary/Layout_viewport)
usually remains fixed in place and represents the size of the page. Elements
with `position: fixed` are attached to the layout viewport. Conversely, the
[Visual Viewport](https://developer.mozilla.org/en-US/docs/Glossary/Visual_Viewport)
represents the screen’s visible slice of the page. If the user zooms, pans or
scrolls, the Visual Viewport may show a different portion of the page.


## What Changed?

**In M136**, WebView added support for
[displayCutout()](https://developer.android.com/reference/androidx/core/view/WindowInsetsCompat.Type#displayCutout())
insets and
[systemBars()](https://developer.android.com/reference/androidx/core/view/WindowInsetsCompat.Type#systemBars())
insets. These were forwarded to the web content via the safe-area-insets CSS
values. Due to compatibility issues, we only supported passing these inset
values to the web content when the WebView was fullscreen.

**Pre-M139**, both WebView’s Layout and Visual Viewports were fixed and
identical. This caused some issues on newer devices as we were unable to
communicate to the web developer which parts of the screen were obscured by the
keyboard. **In M139**, WebView adopted support for the
[ime()](https://developer.android.com/reference/androidx/core/view/WindowInsetsCompat.Type#ime())
inset type so that the Visual Viewport could be resized independently of the
Layout Viewport. In the same milestone, WebView added logic to resize the visual
viewport based on which portion of the WebView is visible to the user. For
example, if the WebView has a height of 3000px but only the top 1000px is
visible on screen, the WebView's visual viewport will be resized to fit the
portion that is visible. This only applies to when the bottom of a WebView moves
off screen; the same is not true if the top, left or right of the WebView moves
past the edge of the screen. At the time of writing, the current level of
support is as follows:

WebView respects the following inset types:

*   displayCutout()
*   systemBars()
*   ime()

displayCutout and systemBars are forwarded to the web content via the
[safe-area-insets](https://developer.mozilla.org/en-US/docs/Web/CSS/Guides/Environment_variables/Using#safe-area-inset-)
API. The ime insets directly affect the size of the Visual Viewport.
**Pre-M139**, the IME would show and sometimes it was impossible to view some
elements which had become hidden. With the IME insets modifying the Visual
Viewport, the visible portion of the page will by default be scrollable so the
user can view the hidden content.

Now that keyboard visibility changes the Visual Viewport, you may see a greater
number of resize events fired in your web code. You should revisit your code to
ensure that it’s not doing something unexpected when the page resizes. We’ve
seen a number of reports where the resize events are clearing the page’s focus.
This makes focusing an input element impossible as the following occurs:

User focuses input -> Keyboard shows -> Resize event is fired -> Page focus is
cleared -> Keyboard is hidden.

**In M144**, we expanded inset support to all WebViews regardless of whether
they are displaying fullscreen or not. The insets should still only report
non-zero values when the UI directly overlaps with WebView’s bounds. For
example, if you have a WebView in the middle of the screen where no part of it
overlaps with the system bars, display cutout or ime, it is the expected
behavior that all the inset values report 0 to the web content. However, when
there is any overlap between the WebView and the display cutout, system bars or
ime, the web content will receive the correct padding values through the
corresponding channel (safe-area-insets or visual viewport resizing).


## What is needed from apps?

Please check your app’s code to ensure that it is handling WindowInsets
correctly. Insets can be handled either by an
[OnApplyWindowInsetsListener](https://developer.android.com/reference/android/view/View.OnApplyWindowInsetsListener)
or via the
[onApplyWindowInsets](https://developer.android.com/reference/android/view/View#onApplyWindowInsets(android.view.WindowInsets))
method; both have almost the same signature (the former has an additional view
parameter).


```
ViewCompat.setOnApplyWindowInsetsListener(webView, (view, insets) -> {
  // Process insets here and return those not consumed by this method.
});

// Alternatively...

class MyWebView extends WebView {
  @Override
  public WindowInsets onApplyWindowInsets(WindowInsets insets) {
    // Process insets here and return those not consumed by this method.
  }
}
```


The WindowInsets are passed to the method where your UI can process them. For
any insets where your app adds accommodating padding, the WindowInsets returned
from your handler should either have those inset types set to zero (preferred)
or consumed (see below). If you return the constant
[WindowInsets.CONSUMED](https://developer.android.com/reference/android/view/WindowInsets#CONSUMED)
from your method, WebView will not send any safe area insets to the web
contents. If you return the insets passed to the method and also apply padding
to your activity, you may see extra spacing in the WebView.

The difference between marking insets as consumed and marking them as zero is
only relevant if the insets consumed by your handler have changed. For example,
if you previously did not add any padding to your activity and then, based on
some external state, started adding padding for one type of inset, you should
set the newly changed insets to zero rather than consuming them. This is because
inset handler calls are propagated down the view hierarchy but traversal stops
when insets are consumed. Therefore, if you marked the newly consumed insets as
CONSUMED instead of setting them to zero, WebView would continue to show the old
insets as it wouldn’t receive any notification to change its internal padding.
See below for an example of a valid insets listener:


```
ViewCompat.setOnApplyWindowInsetsListener(rootView, (view, windowInsets) -> {
  int types = WindowInsetsCompat.Type.systemBars()
              | WindowInsetsCompat.Type.displayCutout();
  Insets insets = windowInsets.getInsets(types);
  rootView.setPadding(insets.left, insets.top, insets.right, insets.bottom);
  // 0 the consumed insets.
  return new WindowInsetsCompat.Builder(windowInsets)
                               .setInsets(types, Insets.NONE)
                               .build();
});
```



## Opt-Out

If you don’t want the new behavior in your app, you can easily opt out. Android
provides the setOnApplyWindowInsetsListener API with which you can intercept and
consume any insets that you don’t want WebView to handle. If you have subclassed
WebView, you can also override onApplyWindowInsets directly and consume the
insets there. Note that if you are consuming these insets, you should ensure
that your UI is properly configured to either resize or add padding so as not to
overlap whatever occlusion the insets are reporting. To force WebView to reset
its insets, do not pass consumed insets. Instead, clear the insets and pass them
to the WebView. This will clear the internal safe-area-insets and will return
the Visual Viewport to its initial size (the size of the Layout Viewport).

If you decide to opt out, some features may not work as expected. The keyboard
may cover inputs near the bottom of the page and in some cases, scrollIntoView
may not work properly.
