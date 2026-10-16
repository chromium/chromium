---
name: general/link-new-docs-in-readme-or-index
rubric: readability
severity: optional
---

**Rule.** When adding a new documentation file or standalone guide in a
component or subsystem directory, add a link to it in the directory's README.md
landing page or the document index (such as docs/README.md). Ensure the entry
includes appropriate context or description so readers can discover and navigate
to the new guide.

**Why.** Subsystem and directory README.md files serve as orientation guides and
primary entry points for developers. Standalone documentation files become
orphaned and difficult to discover if they are not referenced from the relevant
directory landing page or index.

## Examples

Bad:

```cpp
<!-- testing/libfuzzer/blackbox_coverage.md is added, but testing/libfuzzer/README.md is left unchanged -->
*   [Blackbox fuzzing] - Fuzz large, slow, or non-deterministic targets without
    coverage guidance.
```

Good:

```cpp
<!-- testing/libfuzzer/README.md -->
*   [Blackbox fuzzing] - Fuzz large, slow, or non-deterministic targets without
    coverage guidance. To generate and view a code coverage report, see
    [Local Code Coverage for blackbox fuzzers].
...
[Local Code Coverage for blackbox fuzzers]: blackbox_coverage.md
```
