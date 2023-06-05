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

This is done by performing a custom link step with a linker that just records
inputs. This seemed like the simplest approach.

Two alternatives:
1) Dump paths found in debug information.
2) Scan a linker map file for input paths.
