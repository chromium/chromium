# cc/input

This directory contains code specific to input handling and scrolling in in the
compositor.

The renderer compositor typically receives, on the compositor thread, all input
events arriving from the browser. In some cases, the compositor can process
input without consulting the main thread. We strive for this since it means
input doesn't have to block on a potentially busy main thread.

If the compositor determines that Blink must be consulted to correctly handle
the event. e.g. For detailed hit-testing or correct paint output. In these
cases, the event will be posted to the Blink main thread.

See [InputHandlerProxy](../../ui/events/blink/input_handler_proxy.cc) for the
entry point to this code.

## Scrolling

### Viewport

Viewport scrolling is special compared to scrolling regular ScrollNodes. The
main difference is that the viewport is composed of two scrollers: the inner
and outer scrollers. These correspond to the visual and layout viewports in
Blink, respectively.

The reason for this composition is pinch-zoom; when a user zooms in, the layout
viewport remains unchanged (position: fixed elements don't stick to the user's
screen) and the user can pan the visual viewport within the layout viewport.
See [this demo](http://bokand.github.io/viewport/index.html) for a visual,
interactive example.

This arrangement requires some special distribution and bubbling of
scroll delta. Additionally, viewport scrolling is also responsible for
overscroll effects like rubber-banding and gestural-navigation as well as URL
bar movement on Android.

Notably, that the UI compositor as well as renderer compositors for
out-of-process iframes will not have an inner or an outer viewport scroll node.

#### Scroll Chain Structure

The inner viewport scroll node is always the first and only child of the root
scroll node; it is the top-level scrollable node in the scroll tree.  The outer
viewport will typically be the one child of the inner viewport scroll node;
however, this may be changed on certain pages. This happens when a page is
given a non-document root scroller. For more information the root
scroller see the
[README](../../third_party/blink/renderer/core/page/scrolling/README.md) in
Blink's core/page/scrolling directory.

#### Scrolling the Viewport

Viewport scroll nodes are typically not scrolled directly, like other scroll
nodes. Instead, they're scrolled by using the cc::Viewport object. cc::Viewport
is an object that's lives on the LayerTreeHostImpl and operates on the active
tree's inner and outer scroll nodes. It encapsulates the bubbling,
distribution, top controls, etc. behavior we associate with scrolling the
viewport.

We use the outer viewport scroll node to represent cc::Viewport scrolling in
cases where the scroller must be represented by a scroll node (e.g.
CurrentlyScrollingNode). In these cases we make sure to check for the outer
scroll node use cc::Viewport instead. This means that in cases where we want
"viewport" scrolling, we must use the outer viewport scroll node. This can also
happen when the inner viewport is reached in the scroll chain, for example, by
scroll bubbling from a `position: fixed` subtree; we use the outer scroll node
to scroll this case.

The scroll chain is terminated once we've scrolled the cc::Viewport. i.e.
scrolls don't bubble above the cc::Viewport.

#### Root Scroller Nuances

When we have a non-document root scroller, there are cases where we
specifically wish to scroll only the inner viewport.  For example, when a
scroll started from a non-descendant of the root scroller or a `position:
fixed` element and bubbles up. In these cases, we shouldn't scroll using
cc::Viewport because that would scroll the root scroller as well. Doing so
would create a difference in how scrolls chain based on which element is the
root scroller, something we must avoid for interop and compatibility reasons.

This means that when we reach the inner viewport scroll node in the scroll
chain we need to know whether to use cc::Viewport or not. Blink sets the
|prevent\_viewport\_scrolling\_from\_inner| bit on the inner viewport scroll
node so that the compositor can know that scrolls bubbling to the inner
viewport should not use the cc::Viewport class.

## Other Docs

* [Blink Scrolling](../../third_party/blink/renderer/core/page/scrolling/README.md)
  provides information about similar concepts in Blink and the web-platform.

## Glossary

### Inner Viewport

Also called the "Visual Viewport" in web/Blink terminology. This is the
viewport the user actually sees and corresponds to the content visible in the
browser window.

### Outer Viewport

Also called the "Layout Viewport" in web/Blink terminology. This is the main
"content scroller" in a given page, typically the document (`<html>`) element.
This is the scroller to which position: fixed elements remain fixed to.

## Compositor threaded scrollbar scrolling
Contact: arakeri@microsoft.com

### Introduction
Scrollbar scrolling using the mouse happens on the main thread in Chromium. If
the main thread is busy (due to reasons like long running JS, etc), scrolling
by clicking on the scrollbar will appear to be janky. To provide a better user
experience, we have enabled off-main-thread scrollbar interaction for composited
scrollers. This frees up the main thread to perform other tasks like processing
javascript, etc. The core principal here is that MouseEvent(s) are converted to
GestureEvent(s) and dispatched in a VSync aligned manner. Choosing this design
also helps with the grand scrolling unification.

### High-level design:

![Image has moved. Contact arakeri@microsoft.com](https://github.com/rahul8805/CompositorThreadedScrollbarDocs/blob/master/designDiag.PNG?raw=true)

### Core Implementation Details:
This is the basic principle:
- A new class called "cc::ScrollbarController" manages the state and behavior
 related to translating Mouse events into GestureScrolls.
- When a kMouseDown arrives at InputHandlerProxy::RouteToTypeSpecificHandler,
 it gets passed to the ScrollbarController to determine if this event will cause
 scrollbar manipulation.
- The ScrollbarController returns enough data to the InputHandlerProxy to inject
 gesture events to the CompositorThreadEventQueue (CTEQ). For example, in the
 case of a mouse down, a GestureScrollBegin(GSB) and a GestureScrollUpdate(GSU)
 are added to the CTEQ.
- Depending on the action, there can be more synthetic GSUs that get added to
 the CTEQ. (For eg: thumb drags).
- The WebInputEvent::kMouseUp is responsible for cleaning up the scroll state.
- GestureScrollBegin gets dispatched first. This sets up the scroll_node and
 other state necessary to begin scrolling in LayerTreeHostImpl::ScrollBegin.
 This is as usual for all gesture based scrolls.
- GestureScrollUpdate(s) get handled next. Scroll deltas get applied to the node
 that was set up during GestureScrollBegin. Depending on the type of scroll,
 this may lead to an animated scroll (eg: LayerTreeHostImpl::ScrollAnimated for
 autoscroll/mouse clicks) or a regular scroll. (eg: LayerTreeHostImpl::ScrollBy
 for thumb drags)
- Finally, the GestureScrollEnd is dispatched and it clears the scrolling state
 (like the CurrentlyScrollingNode) and calls SetNeedsCommitOnImplThread().

### Miscellaneous resources.
- [Demo page](https://rahul8805.github.io/DemoPages/BouncyMoon.html)
- [Lightning talk](https://www.youtube.com/watch?v=FOCHCuGA_MA&t=1150s)
- [input-dev thread](https://groups.google.com/a/chromium.org/forum/#!topic/input-dev/6ACOSDoAik4)
- [Full design doc](https://docs.google.com/document/d/1JqykSXnCkqwA1E3bUhhIi-IgEvM9HZdKtIu_S4Ncm6o/edit#heading=h.agf18oiankjh)
