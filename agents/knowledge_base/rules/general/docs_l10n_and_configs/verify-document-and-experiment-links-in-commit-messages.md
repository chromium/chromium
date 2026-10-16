---
name: general/verify-document-and-experiment-links-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** When referencing design docs, launch approvals, benchmark evaluations,
or experiment results in commit messages, verify that embedded document URLs
point directly to the specific resource containing the data, analysis, and
architectural rationale for the change. Do not include copy-pasted or
placeholder links to unrelated projects, prior experiments, or disparate design
documents.

**Why.** Commit descriptions form part of the permanent version control history
used during bisection, cherry-picks, and regression investigations. Linking to
unrelated documents or pasting incorrect document URLs misleads reviewers and
future maintainers seeking the technical motivation, launch sign-offs, or metric
tradeoffs behind a feature.

## Examples

Bad:

```cpp
// LowerPriorityForCompositorGestures CL description:
Detailed results:
https://docs.google.com/document/d/1pwHR3kxtuYs0OfStxyxtLSqmhorgQ2YUCRUjeob0yls/  # Unrelated doc ("Hybrid 60/120Hz Rendering")
```

Good:

```cpp
// LowerPriorityForCompositorGestures CL description:
Detailed results:
https://docs.google.com/document/d/1gHXDKdQyeJvagObvK2GYyttHO3aKlsAWDzb_mSlf-10/  # Accurate doc containing LowerPriorityForCompositorGestures data
```
