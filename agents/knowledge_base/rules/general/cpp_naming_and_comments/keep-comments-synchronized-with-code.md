---
name: general/keep-comments-synchronized-with-code
rubric: readability
severity: should_fix
---

**Rule.** Ensure all code, function, class, and test comments and technical
documentation (including component READMEs) accurately reflect current
implementation, parameter signatures, and behavior. Write evergreen descriptions
by stating versions, milestones, and requirements factually without ephemeral
time-relative claims, and prune obsolete fallback instructions, compatibility
notes for past milestones, completed TODOs, and commented-out code. Do not write
code comments that describe revision history or past states of the code (such as
explaining what a function or test did before a refactoring or language
upgrade); document current requirements and rationale instead, leaving history
to version control.

**Why.** Inaccurate, stale, or obsolete comments and documentation (such as
legacy milestone workarounds or contradictory descriptions) clutter the codebase
and mislead maintainers about active behavior and contracts. Documenting
revision history in source code adds ephemeral noise that quickly goes stale and
duplicates information already tracked in version control history.

## Examples

Bad:

```cpp
// Variant 1: Stale / contradictory comment after reverting or changing code
SetTlsVectorValue(key, data, TlsVectorState::kInUse);  // State set to kDestroying to skip deletion.

// Variant 2: Documenting revision history / past implementations
// This test originally tested that EnumSets failed to construct in constexpr
// contexts with >64 values before C++23 constructor changes were introduced.
void TestLargeEnumSet() { ... }

// Variant 3: Ephemeral milestone claims in comments
# A trace from M132 (ToT as of adding this test) has the necessary events/arguments.
```

Good:

```cpp
// Variant 1: Accurate synchronized comment
SetTlsVectorValue(key, data, TlsVectorState::kInUse);  // Initialize with stack-allocated vector.

// Variant 2: Document current requirements/invariants without revision history
// Verify EnumSet supports constexpr evaluation with large enum ranges.
void TestLargeEnumSet() { ... }

// Variant 3: Factual, evergreen comment without ephemeral claims
# A trace from M132 has the necessary events and arguments.
```
