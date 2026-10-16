---
name: general/replace-tool-boilerplate-with-accurate-commit-description
rubric: readability
severity: should_fix
---

**Rule.** Ensure the commit message description accurately and clearly explains
what was changed and why, providing an informative narrative of the actual
changes. Remove automated tool boilerplate, script invocation commands (e.g.,
clang spanify scripts), batch run IDs (`gemini-run/batch-run-...`), obsolete
templates, stray commit hashes, raw git log listings, squash/fixup markers,
temporary scratchpad notes (`TODO:`, `WIP:`), and squashed commit blocks
separated by divider lines (`-----`). Consolidate multi-step or tool-generated
changes into a cohesive description, keeping all Git footers strictly in the
final paragraph.

**Why.** The CL description becomes the permanent commit message in repository
version control history, used by reviewers, sheriffs, and maintainers during
regression triage and code archaeology. Leaving automated tool boilerplate, raw
script output, batch IDs, stray commit hashes, or scratchpad notes obscures the
true intent and scope of the patch and can break metadata footer parsing in
Gerrit.

## Examples

Bad:

```cpp
// Anti-pattern 1: Tool boilerplate, script invocations, and batch run IDs
spanification: automatically spanify base/metrics/histogram.cc etc.

This is the result of running the automatic spanification on linux and
updating code to use and pass spans where size is known.

The original patch was fully automated using script:
//tools/clang/spanify/rewrite-multiple-platforms.sh -platforms=linux
Then refined with gemini-cli

gemini-run/batch-run-1761116551/group_126

BUG=439964610

// Anti-pattern 2: Stray hashes, scratchpad notes, and squashed dividers
Add feature flag for new sync protocol.

TODO: clean up flag before branch cut
05269adf5324602ab395782d60fdb7f99c84db44 update license
-----
Modernize `log_severity_names` to use `constexpr std::array`.

Bug: 447871698
Change-Id: Ie91748d6b74840502472492e1b5ad3d8498f494c
```

Good:

```cpp
// Pattern 1: Clear narrative explaining what changed and why
LinearHistogram: Remove unused descriptions parameter

The `descriptions` parameter in
`LinearHistogram::FactoryGetWithRangeDescription` is no longer used
by any callers. Removing this parameter allows deleting several
private members and helper functions that exclusively supported it.

Bug: 439964610

// Pattern 2: Consolidated description body with footers in final paragraph
Add feature flag for new sync protocol.

This CL introduces the base feature flag for the next-generation sync
protocol implementation and modernizes `log_severity_names` to use
`constexpr std::array`.

Bug: 447871698
Change-Id: Ie91748d6b74840502472492e1b5ad3d8498f494c
```
