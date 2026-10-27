# Private Code Test

This directory provides a mechanism for testing that native does not link in
object files from unwanted directories. The test finds all linker inputs, and
checks that none live inside a list of internal paths.

Original bug: https://bugs.chromium.org/p/chromium/issues/detail?id=1266989

## Determining Internal Directories

This is done by parsing `//DEPS` (and checked-out internal `recursedeps` such as
`//clank/DEPS` and `//components/optimization_guide/internal/DEPS`) for all
`deps` entries coming from https://chrome-internal.googlesource.com. Conditions
in `DEPS` files are not evaluated (evaluating them would require `.gclient`
`custom_vars` outside `src/`, as `checkout_src_internal` defaults to `False` in
`//DEPS`). Including all potential internal paths regardless of the local
checkout configuration is harmless because `private_code_test.py` only checks
actual linker inputs against this list—any conditional path not present in the
checkout simply won't match any linker inputs.

Because Siso remote execution requires all action inputs to be statically
declared in GN and some `recursedeps` repositories are only conditionally
checked out, `list_gclient_deps.py` does not dynamically discover `recursedeps`
on disk. Instead:
* Conditionally checked-out internal `recursedeps` (`//clank/DEPS` and
  `//components/optimization_guide/internal/DEPS`) are passed from `BUILD.gn`
  when `gclient_args.gni` indicates they are present. (Their parent directories
  are also already listed as top-level `chrome-internal` entries in `//DEPS`, so
  any sub-dependencies scoped under them via `use_relative_paths = True` are
  already covered by prefix matching.)
* `list_gclient_deps.py` validates `recursedeps` against an audited allowlist
  and fails the test if any new `recursedeps` entry is added without updating
  the script.

Alternatives considered:

1) Parse `.gclient_entries`.
  * This was the original approach because the file is trivial to parse and
    already flattens `recursedeps`, but `.gclient_entries` lives outside the
    `src/` checkout root, which breaks remote execution and build path
    virtualization.
2) Use `gclient flatten`. Example output:

```
  # src -> src/internal
  "src/internal": {
    "url": "https://chrome-internal.googlesource.com/chrome/src-internal.git@c649c6a155fe65c3730e2d663d7d2058d33bf1f9",
    "condition": 'checkout_src_internal',
  },
```

  * Paths could be found in this way by looking for `checkout_src_internal`
    within `condition`, and by looking for the comment line for `recursedeps`
    that went through an internal repo. However, invoking `gclient` also relies
    on `.gclient` state outside the `src/` checkout root.

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
