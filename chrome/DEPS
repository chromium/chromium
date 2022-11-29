include_rules = [
  "+crypto",
  "+gpu",
  "+net",
  "+pdf/buildflags.h",
  "+printing",
  "+sandbox",
  "+sandbox/policy",
  "+sql",
  # Browser, renderer, common and tests access V8 for various purposes.
  "-v8",
  "+v8/include",

  # Limit what we include from nacl.
  "-native_client",

  # Individual subdirectories of chrome/ must explicitly declare their
  # dependencies on other subdirectories of chrome/.
  "-chrome",

  "+chrome/common",
  "+chrome/test",
  "+components/content_settings/common",
  "+components/content_settings/core/common",
  "+components/embedder_support/switches.h",
  "+components/error_page/common",
  "+components/media_router/common",
  "+components/omnibox/common",
  "+components/services/heap_profiling/public",
  "+components/url_formatter",
  "+components/variations",
  "+content/public/common",
  "+content/public/test",
  "+media/media_buildflags.h",
  "+mojo/public",
  "+ppapi/buildflags",

  # Don't allow inclusion of these other libs we shouldn't be calling directly.
  "-webkit",
  "-tools",

  # Required for process launching.
  "+services/service_manager",

  # Allow inclusion of WebKit API files.
  "+third_party/blink/public/common",
  "+third_party/blink/public/platform",
  "+third_party/blink/public/public_buildflags.h",
  "+third_party/blink/public/web",

  # Allow inclusion of third-party code:
  "+third_party/hunspell",
  "+third_party/skia",

  "+ui",
]
