# Static Library Java code

[TOC]

## Overview

This document describes how static library targets can be used to share common
Java code between multiple APKs. More detail can be found at
[go/proguarding-trichrome](goto.google.com/proguarding-trichrome).

## TrichromeLibrary

Currently (Jan 2020) trichrome library is the only target to make use of static
shared library APKs and is used to share common code used by both Chrome and
Webview.

## Status

Java code sharing is mostly implemented at this point but there is one remaining
blocker related to how
[native method resolution works in Webview](crbug.com/1025009).

## How it works

### Build variables

For `android_apk_or_module` base templates:

`static_library_provider`: Specifies that this target depends on a static shared
library APK. When synchronized proguard is turned on, the
`static_library_provider` becomes the target that provides the final dex file.

`static_library_dependent_targets`: If set, generates final dex files for
itself and for all targets in the `static_library_dependent_targets` list.

`static_library_synchronized_proguard`: Turns on synchronized proguard for
targets that also set `static_library_provider`.

### .build_config

`write_build_config.py` is responsible for figuring out where code and related
artifacts for the `static_library_provider` and
`static_library_dependent_targets` belongs. The main difference from regular
`.build_configs` is the mapping recording which input jars belong to each final
dex file. Ex:

```
"deps_info": {
  ...
  "static_library_dependent_classpath_configs": {
      "gen/android_webview/trichrome_webview_apk.build_config.json": [
        "obj/android_webview/trichrome_webview_apk/trichrome_webview_apk.jar",
        ...
      ],
      "gen/chrome/android/trichrome_chrome_bundle.build_config.json": [
        "lib.java/chrome/android/app_hooks_java.jar",
        ...
      "gen/chrome/android/trichrome_library_apk.build_config.json": [
        "lib.java/base/base_java.jar",
        ...
      ]
      ...
  }
}
```

### Synchronized ProGuard

TrichromeChromeBundle (base module) and TrichromeWebview do not have a final
`dex` or `proguard` step. Instead the library APK creates a "fat" dex from the
`.build_config.json:deps_info:java_runtime_classpath`.

Then, the mapping of `.build_config.json` -> owned input jars stored in the
`.build_config.json` is used by `dexsplitter` to generate final .dex files for
TrichromeLibrary, TrichromeChrome, and TrichromeWebview.

### Resources

For Java code to be shared between Chrome and Webview in [Trichrome][trichrome],
we ensure that Chrome and Webview use the same resource IDs. This requires a
few adjustments to how resources are created.

1. Webview's resources are compiled first without any changes.
2. Chrome's resources are compiled second, but use the same resource IDs as
   Webview when possible.
3. When synchronized proguarding is turned on, the `R.java` files generated in
   the previous step are discarded. The shared static library APK target
   (trichrome library) takes the output `R.txt` files from the previous steps
   and includes those resources in its own `R.java` generation.

[trichrome]: /chrome/android/trichrome/static_library_shared_java_code.md

### Usage

* Building trichrome_chrome_bundle or trichrome_webview_apk (and various arch
  variants) will ensure the correct library target is also built.
* Using the generated wrapper script from the main APK is sufficient (no need
  to explicitly install the library).
  * `bin/trichrome_chrome_bundle run` will ensure TrichromeChromeBundle and
    TrichromeLibrary are installed before launching Chrome.
