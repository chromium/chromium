# Overhauled performance tracing in Chrome

We are upgrading Chrome's support for performance tracing by replacing Chrome's implementation of
TRACE_EVENT macros from //base/trace_event with [Perfetto](https://perfetto.dev). Perfetto
introduces [trace events with typed
arguments](https://perfetto.dev/docs/instrumentation/track-events) to support privacy-filtered trace
recording and a more compact, efficient, and stable trace encoding.

The Perfetto library itself lives in
[AOSP](https://android.googlesource.com/platform/external/perfetto/) and is rolled in
[/third_party/chrome/](https://cs.chromium.org/chromium/src/third_party/perfetto/). Progress is
tracked on https://crbug.com/1006541.

The code in this directory connects Chrome to Perfetto's [tracing
SDK](https://perfetto.dev/docs/instrumentation/tracing-sdk), which implements trace event macros on
top of Perfetto's [tracing service](https://perfetto.dev/docs/concepts/service-model). This service
can be run in-process (e.g. in unit tests), as a Chrome mojo service (see //services/tracing), or as
a system service on Android.

For more details, see [Perfetto's documentation](https://docs.perfetto.dev), [Typed trace events in
Chrome](https://docs.google.com/document/d/1f7tt4cb-JcA5bQFR1oXk60ncJPpkL02_Hi_Bc6MfTQk/edit#), and
[Typed trace events in
//base](https://docs.google.com/document/d/1UQ4Ez7B-TeowijOUuMXuoWj1amZcQ7E2abt3s4jaAEY/edit#).
