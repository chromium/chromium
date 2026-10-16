---
name: general/combine-multiple-repo-scans-in-presubmit-checks
rubric: safety
severity: should_fix
---

**Rule.** In presubmit checks and repository-scanning tools, avoid performing
multiple independent full-repository file scans or directory traversals for
separate checks. Combine related inspections and symbol extractions into a
single traversal pass or shared worker pool that extracts all required metadata
simultaneously.

**Why.** Scanning the entire Chromium repository involves traversing tens of
thousands of files and adds substantial latency to local presubmit runs and
commit queue bots. Consolidating inspections into a single traversal minimizes
disk I/O overhead and presubmit latency.

## Examples

Bad:

```cpp
if input_api.is_committing:
  # Bad: two separate full repository scans traverse tens of thousands of files twice.
  result = CheckUndeclaredFeatures(input_api, output_api, json_data, changed_lines)
  if result:
    return result
  result = CheckUndeclaredFeatureParams(input_api, output_api, json_data, changed_lines)
  if result:
    return result
```

Good:

```cpp
if input_api.is_committing:
  # Good: combine multiple repository-wide extractions into a single traversal pass.
  declared_features, declared_params = (
      find_features.FindDeclaredFeaturesAndParams(input_api))
```
