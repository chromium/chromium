---
name: general/annotate-test-widened-members-with-visiblefortesting
rubric: readability
severity: should_fix
---

**Rule.** When a production member is widened from `private` only so that tests
can reach it, and its name has no `ForTesting` suffix, annotate it with
`@VisibleForTesting`. If it was made `public` for a test in another package, use
`@VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)` or the
narrowest real visibility. Make the member `private` again once no test uses it,
and before adding a `get*ForTesting()` accessor check whether the test can use
an existing getter. Do not ask for `@VisibleForTesting` on a `*ForTesting`
method: the suffix already marks it, and the
`CheckNoProductionCodeUsingTestOnlyFunctionsJava` presubmit flags production
callers of it.

**Why.** Without the annotation nothing tells readers or tools that the wider
visibility exists only for tests, so production code starts depending on a
member that was never meant to be part of the class's API.

## Examples

Bad:

```java
// Package-private only so that FooMediatorTest can call it.
void notifyPreferencesUpdated() { ... }
```

Good:

```java
@VisibleForTesting
void notifyPreferencesUpdated() { ... }
```
