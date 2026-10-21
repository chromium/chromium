---
name: third_party/do-not-modify-third-party-submodule-source
rubric: readability
severity: optional
---

**Rule.** For third-party dependencies managed as Git submodules (such as via
Skia Autoroller), pull in the entire upstream repository into
third_party/<name>/src/ and do not modify upstream source files to satisfy
Chromium conventions. Place all Chromium integration files (such as BUILD.gn,
README.chromium, wrapper headers, and tests) directly alongside the src/
directory; if only a subset of upstream files or source modifications are
required, use a checked-in code workflow (such as Crowbar) instead.

**Why.** Git submodule mirroring via Skia Autoroller does not support sparse
checkouts or importing partial subsets of repositories. Keeping
third_party/<name>/src/ as a complete, pristine upstream mirror ensures
automated rolling tools can update revisions cleanly without merge conflicts or
diverging forks.

## Examples

Bad:

```cpp
// 1. In third_party/<name>/src/upstream_file.cc:
// Modifying upstream source directly in a submodule checkout:
#include "base/containers/span.h"
base::span<const uint8_t> buffer;

// 2. In submodule configuration / documentation:
// Attempting to pull in only a subset of files/directories via sparse checkout:
// Skia Autoroller Git submodule mirroring does not support partial imports.
```

Good:

```cpp
// 1. Keep third_party/<name>/src/ as a pristine full-repo upstream checkout.
// Place Chromium integration files directly alongside src/:
// third_party/<name>/BUILD.gn
// third_party/<name>/include/directml.h  (header wrapper/guard)

// 2. If only a subset of upstream files or local source modifications are needed,
// use a checked-in code workflow (such as Crowbar) instead of a Git submodule.
```
