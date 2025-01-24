# Private Code Test

This directory provides a mechanism for testing that native does not link in
object files from unwanted directories. The test finds all linker inputs, and
checks that none live inside a list of internal paths.

Original bug: https://bugs.chromium.org/p/chromium/issues/detail?id=1266989

## Determining Internal Directories

This is done by parsing the `.gclient_entries` file for all paths coming from
https://chrome-internal.googlesource.com. I chose this approach since it is
simple.

The main alternative I found was to use `gclient flatten`. Example output:

```
  # src -> src/internal
  "src/internal": {
    "url": "https://chrome-internal.googlesource.com/chrome/src-internal.git@c649c6a155fe65c3730e2d663d7d2058d33bf1f9",
    "condition": 'checkout_src_internal',
  },
```

* Paths could be found in this way by looking for `checkout_src_internal`
  within `condition`, and by looking for the comment line for `recurse_deps`
  that went through an internal repo.

## Determining Linker Inputs

This is done by parsing `build.ninja` to find all inputs to an executable. This
approach is pretty fast & simple, but does not catch the case where a public
`.cc` file has an `#include` a private `.h` file.

Alternatives considered:

1) Dump paths found in debug information.
  * Hard to do cross-platform.
2) Scan a linker map file for input paths.
  * LTO causes paths in linker map to be inaccurate.
3) Use a fake link step to capture all object file inputs
  * Object files paths are relative to GN target, so this does not catch
    internal sources referenced by public GN targets.
4) Query GN / Ninja for transitive inputs
  * This ends up listing non-linker inputs as well, which we do not want.
5) Parse depfiles to find all headers, and add them to the list of inputs
  * Additional work, but would give us full coverage.
