---
name: general/use-std-move-over-std-forward-for-local-variables
rubric: readability
severity: should_fix
---

**Rule.** Do not use std::forward on local variables or expiring values when
passing them to functions, callbacks, or return statements; use std::move
instead. Reserve std::forward exclusively for universal/forwarding reference
parameters (T&&).

**Why.** std::forward<T> is designed for conditional rvalue casting of
forwarding reference parameters depending on deduced template type arguments.
For local variables whose lifetime is ending, unconditional transfer is
intended; std::move communicates this clearly and binds correctly to by-value,
rvalue reference (T&&), and const reference (const T&) parameters.

## Examples

Bad:

```cpp
// Bad: Misusing std::forward on a local variable.
UnqualifiedArg result = jni_zero::FromJniType<UnqualifiedArg>(env, j_result);
std::move(captured_callback).Run(std::forward<Arg&&>(result));
```

Good:

```cpp
// Good: Use std::move on local variables; reserve std::forward for forwarding references.
UnqualifiedArg result = jni_zero::FromJniType<UnqualifiedArg>(env, j_result);
std::move(captured_callback).Run(std::move(result));
```
