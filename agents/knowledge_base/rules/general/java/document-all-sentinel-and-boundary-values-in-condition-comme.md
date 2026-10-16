---
name: general/document-all-sentinel-and-boundary-values-in-condition-comme
rubric: readability
severity: nit
---

**Rule.** When writing an explanatory comment for a condition or loop
termination check that evaluates multiple sentinel values, return codes, or
boundary thresholds (such as an inequality or compound comparison), explicitly
document the meaning of each value or case covered by the check. Do not explain
only a subset of the sentinel values or leave boundary thresholds ambiguous.

**Why.** Explaining only one sentinel value or return case in a comment while
checking a range or compound condition (such as explaining failure returning 0
while checking `<= 1`) creates ambiguity about why that boundary was chosen.
Documenting every covered sentinel value and threshold clarifies the complete
control-flow contract and prevents maintainers from misunderstanding or altering
intentional bounds.

## Examples

Bad:

```cpp
// Stop if we hit system root or if GetParentPid fails (returns 0).
if (parent_pid <= 1) {
  break;
}

// Bail out if the operation failed (error code 0).
if (result_code <= 1) {
  return false;
}
```

Good:

```cpp
// Stop if we hit system root (GetParentPid returns 1) or if GetParentPid
// fails (returns 0).
if (parent_pid <= 1) {
  break;
}

// Bail out if the operation is cancelled (code 1) or failed (code 0).
if (result_code <= 1) {
  return false;
}
```
