# Lifetime annotations

## Overview

As part of the WebView Multi-profile project, classes and state in
`//android_webview/`` will be annotated to make their lifetimes explicit.
There are broadly 5 different lifetimes that a class can have:

- Singleton - there is one instance of this class per WebView browser process.
  As WebView is run in the embedding app's process, this means there is one
  instance per embedding app.
- Scoped to Profile - this class is specific to a Profile. It is likely to do
  with the user's browsing state, such as cookies or permissions.
- Scoped to WebView - this class is specific to a WebView (the Android View),
  for example the AwZoomControls.
- Temporary - this class has a shorter lifetime than a WebView. Examples include
  objects that live within a single call stack, objects associated with
  a single HTTP request or page navigation, etc.
- Renderer - this class has a lifetime tied to specific to a Renderer.

There is a many to one relationship between WebViews and Profiles. A single
Profile can support multiple WebViews, each WebView will only have a single
(constant) Profile.

These annotations are purely for documentation, there is no static analysis to
check them.

## Annotations

In Java, use the following annotations:

- `@Lifetime.Singleton`
- `@Lifetime.Profile`
- `@Lifetime.WebView`
- `@Lifetime.Temporary`
- `@Lifetime.Renderer`

In C++, use the following comment format as the last line of the class
documentation:

```
// Lifetime: Singleton
// Lifetime: Profile
// Lifetime: WebView
// Lifetime: Temporary
// Lifetime: Renderer
```
