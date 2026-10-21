---
name: base/document-perfetto-diff-test-command-in-commit-message
rubric: readability
severity: should_fix
---

**Rule.** When adding or modifying Perfetto diff tests, Chrome stdlib metrics,
or test expectations under `base/tracing`, document the local command used to
build and execute the diff tests (e.g.,
`autoninja -C out/Linux perfetto_diff_tests && out/Linux/bin/run_perfetto_diff_tests`)
in the CL description.

**Why.** Perfetto diff tests run on Linux FYI builders (`chromium.fyi`) rather
than standard CQ trybots. Documenting the local execution command in the CL
description confirms that test changes were verified locally and allows
reviewers and future maintainers to easily reproduce test results.

## Examples

Bad:

```cpp
Update test expectations to not rely on track_ids

This backports tests_scroll_jank.py changes from
https://github.com/google/perfetto/pull/2145
to make sure tests no longer rely on track_ids.
```

Good:

```cpp
Update test expectations to not rely on track_ids

This backports tests_scroll_jank.py changes from
https://github.com/google/perfetto/pull/2145
to make sure tests no longer rely on track_ids.

PERFETTO_TESTS=`autoninja -C out/linux perfetto_diff_tests && out/linux/bin/run_perfetto_diff_tests`
```
