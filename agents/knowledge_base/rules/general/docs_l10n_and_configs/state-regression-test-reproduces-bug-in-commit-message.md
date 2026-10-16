---
name: general/state-regression-test-reproduces-bug-in-commit-message
rubric: readability
severity: should_fix
---

**Rule.** When adding unit or regression tests, modifying test targets, or
adjusting test configurations alongside bug fixes, refactorings, or new
features, explicitly describe the test coverage and verification purpose in the
commit message body. For bug fixes or crash resolutions, state that the
regression test reproduces the defect and fails prior to the patch. For
secondary test or configuration edits made alongside new features or stubs (such
as adding attributes to existing targets to verify they are accepted or
ignored), explain their verification purpose so readers distinguish them from
active adoptions or accidental inclusions.

**Why.** Commit messages serve as the permanent record in Git history.
Explicitly noting added test coverage and explaining secondary target
modifications provides immediate verification context and prevents confusing
test verifications with unintended edits, while confirming defect reproduction
ensures regression tests actively validate fixes.

## Examples

Bad:

```cpp
// Anti-pattern 1: Omitting test coverage details from the commit message
Fix pointer/reference typo in RepeatingCallback::operator=

The DoNothingWithBoundArgs assignment operator returned `this` (a
pointer) instead of `*this` (a reference).

Bug: 40012345
Change-Id: I1234567890abcdef1234567890abcdef12345678

// Anti-pattern 2: Vaguely asserting test coverage without confirming repro
Fix crash in CallbackList when cancelling callback during reentrant notify

The newly-added unit test CallbackListTest.OnceCallbackListCancelDuringReentrantNotify
covers this scenario.

Bug: 40012346
Change-Id: I47201ef5b84260fa1185458da9b08f51a2384a26

// Anti-pattern 3: Omitting verification rationale for secondary test modifications
[Fuzzing] Add stub for "is_fuzztest_compatible" attribute

crrev.com/c/7737502 implements this attribute but this stub is needed so
that a downstream package can set the attribute before one of its
fuzzers is broken by the wrapper.

Bug: 40012347
Change-Id: Iabcdef1234567890abcdef1234567890abcdef12
```

Good:

```cpp
// Pattern 1: Explicitly noting added unit test coverage
Fix pointer/reference typo in RepeatingCallback::operator=

The DoNothingWithBoundArgs assignment operator returned `this` (a
pointer) instead of `*this` (a reference).

This CL fixes the return type and adds unit test coverage for operator=.

Bug: 40012345
Change-Id: I1234567890abcdef1234567890abcdef12345678

// Pattern 2: Confirming regression test reproduces defect prior to patch
Fix crash in CallbackList when cancelling callback during reentrant notify

The newly-added unit test CallbackListTest.OnceCallbackListCancelDuringReentrantNotify
reproduces the crash and failed prior to this patch.

Bug: 40012346
Change-Id: I47201ef5b84260fa1185458da9b08f51a2384a26

// Pattern 3: Explaining purpose of secondary test or config target edits
[Fuzzing] Add stub for "is_fuzztest_compatible" attribute

crrev.com/c/7737502 implements this attribute but this stub is needed so
that a downstream package can set the attribute before one of its
fuzzers is broken by the wrapper.

The attribute is added to "string_compare_fuzzer" to verify that it is
correctly ignored.

Bug: 40012347
Change-Id: Iabcdef1234567890abcdef1234567890abcdef12
```
