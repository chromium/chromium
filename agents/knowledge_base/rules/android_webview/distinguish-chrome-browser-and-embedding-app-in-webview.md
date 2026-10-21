---
name: android_webview/distinguish-chrome-browser-and-embedding-app-in-webview
rubric: readability
severity: should_fix
---

**Rule.** In WebView CL descriptions, documentation, and code comments, do not
use the ambiguous phrase 'browser app'. Explicitly distinguish between the
standalone 'Chrome browser' (or Chrome app), the 'embedding app' (or host app),
and the 'browser process' (the host application process where WebView
browser-tier code executes).

**Why.** In Android WebView architecture, WebView browser code runs inside the
embedding host application's address space as the browser process. Referring to
a 'browser app' creates ambiguity between the standalone Chrome browser product,
the third-party embedding application, and the internal browser process,
obscuring whether embedder developers must configure host settings or if WebView
is simply aligning with Chrome.

## Examples

Bad:

```cpp
This change ensures that the lock spin count is configured in both the
browser app and WebView.
```

Good:

```cpp
This change ensures that the lock spin count is configured for WebView,
aligning its behavior with the Chrome browser.

The lock spin count is now initialized across all relevant processes
(browser, renderer, and GPU) at startup as part of a field trial.
```
