---
name: general/exclude-symbol-definition-files-from-presubmit-reference-ban
rubric: correctness
severity: should_fix
---

**Rule.** When writing or updating presubmit checks that ban direct references
to a symbol, API, or feature flag (directing callers to an authorized accessor
or wrapper), explicitly exclude the file(s) where the symbol is declared or
defined, the wrapper implementation itself, and presubmit test files from the
scanned file list.

**Why.** Presubmit checks scan changed lines in modified files. If the file
where the symbol or flag is declared is not exempted, updating or maintaining
the declaration triggers false-positive presubmit errors on the symbol's own
definition.

## Examples

Bad:

```cpp
for f in input_api.AffectedFiles(include_deletes=False):
    local_path = f.LocalPath()
    if ('AndroidSidePanelEnabledFn.java' in local_path
            or 'PRESUBMIT.py' in local_path
            or 'PRESUBMIT_test.py' in local_path):
        continue
```

Good:

```cpp
for f in input_api.AffectedFiles(include_deletes=False):
    local_path = f.LocalPath()
    if ('AndroidSidePanelEnabledFn.java' in local_path
            or 'ChromeFeatureList.java' in local_path
            or 'PRESUBMIT.py' in local_path
            or 'PRESUBMIT_test.py' in local_path):
        continue
```
