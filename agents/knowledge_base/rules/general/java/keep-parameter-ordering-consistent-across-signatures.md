---
name: general/keep-parameter-ordering-consistent-across-signatures
rubric: readability
severity: nit
---

**Rule.** When defining a helper class constructor, factory method, or
forwarding wrapper that accepts the same parameters as an enclosing or wrapped
method, maintain consistent parameter ordering across both signatures. Do not
shuffle or invert the sequence of shared parameters between caller and callee.

**Why.** Inconsistent parameter ordering between enclosing methods and helper
constructors or delegates increases cognitive overhead for readers and creates
subtle argument-swapping bugs at call sites, especially when parameters share
identical or compatible types.

## Examples

Bad:

```cpp
// Anti-pattern: Parameter ordering in HookAftersStatement differs from withAfters
@Override
protected Statement withAfters(FrameworkMethod method, Object test, Statement base) {
    Statement afters = super.withAfters(method, test, new ScreenshotOnFailureStatement(base));
    return new HookAftersStatement(afters, method, test);
}

private static class HookAftersStatement extends Statement {
    public HookAftersStatement(Statement base, FrameworkMethod method, Object test) { ... }
}
```

Good:

```cpp
// Idiomatic: Parameter ordering matches across withAfters and HookAftersStatement
@Override
protected Statement withAfters(FrameworkMethod method, Object test, Statement base) {
    Statement afters = super.withAfters(method, test, new ScreenshotOnFailureStatement(base));
    return new HookAftersStatement(method, test, afters);
}

private static class HookAftersStatement extends Statement {
    public HookAftersStatement(FrameworkMethod method, Object test, Statement base) { ... }
}
```
