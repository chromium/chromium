---
name: general/regression-tests-must-fail-without-the-fix
rubric: testing
severity: should_fix
---

**Rule.** When a CL adds a guard, branch or bug fix together with a test, check
that the test would fail if the production change were reverted. Watch for the
common reasons it passes anyway: an unstubbed Mockito query returns
`false`/`0`/`null` and already satisfies the new condition; an earlier early
return short-circuits before the new check (a `View` that is never attached,
stale dirty state, a fallback path in the fixture); or the setup order caches
state before the test changes it. The test must drive the production code path,
not a test-local copy of its logic.

**Why.** A regression test that also passes on the pre-fix code protects nothing
and gives false confidence that the bug cannot come back.

## Examples

Bad:

```java
// Fix: skip the update when the policy disallows it.
@Test
public void testNoUpdateWhenPolicyDisallows() {
    // mPolicy is a mock; isAllowed() is unstubbed and returns false, so the
    // pre-fix code never updated either.
    mMediator.onTabChanged(mTab);
    verify(mView, never()).update(any());
}
```

Good:

```java
@Test
public void testNoUpdateWhenPolicyDisallows() {
    when(mPolicy.isAllowed()).thenReturn(true);
    mMediator.onTabChanged(mTab);
    verify(mView).update(any()); // The path is reachable.

    clearInvocations(mView);
    when(mPolicy.isAllowed()).thenReturn(false);
    mMediator.onTabChanged(mTab);
    verify(mView, never()).update(any());
}
```
