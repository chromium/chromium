---
name: general/use-assumenonnull-only-before-immediate-dereference
rubric: correctness
severity: should_fix
---

**Rule.** Use `assumeNonNull(x)` only where `x` is dereferenced right away, or
as a statement after an indirect precondition check that NullAway cannot follow.
When the value is instead passed to a non-null parameter or stored in a field,
use `assertNonNull(x)` (or `assert x != null`). Do not add `assumeNonNull(x)`
after an explicit `x != null` check or `assert x != null` on the same value,
because NullAway has already narrowed it. Passing a `@Nullable` `JniPtr`
receiver to a generated `@NativeMethods` proxy counts as an immediate check,
because the proxy asserts that its receiver is non-null.

**Why.** `org.chromium.build.NullUtil.assumeNonNull()` only tells NullAway that
the value is non-null and performs no runtime check; its Javadoc says to prefer
`assertNonNull()` when the value is not dereferenced immediately. Forwarding an
unchecked value moves a possible null to a later crash site that is harder to
debug, and a call after an explicit check is dead code that readers mistake for
a guard.

## Examples

Bad:

```java
if (mTab == null) return;
assumeNonNull(mTab).reload();
mHelper = new Helper(assumeNonNull(mProfile));
```

Good:

```java
if (mTab == null) return;
mTab.reload();
mHelper = new Helper(assertNonNull(mProfile));
```
