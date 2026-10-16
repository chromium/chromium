---
name: general/use-parameter-name-comments-in-java
rubric: readability
severity: nit
---

**Rule.** In Java method and constructor calls, use parameter name comments
formatted as `/* paramName= */` when passing literal arguments (such as `null`,
`true`, `false`, or numeric constants) whose purpose is non-obvious from the
call site. Place the comment immediately before the argument value.

**Why.** Passing bare literal arguments like `null` or boolean flags obscures
the purpose and role of arguments at call sites, forcing readers to inspect
method declarations. Parameter name comments make calls self-documenting and
allow static analysis tools (like Error Prone) to verify argument ordering.

## Examples

Bad:

```cpp
public UnownedUserDataKey(Class<T> clazz) {
    this(clazz, null);
}

public void configureProfile() {
    setupProfile(true, false, null);
}
```

Good:

```cpp
public UnownedUserDataKey(Class<T> clazz) {
    this(clazz, /* listener= */ null);
}

public void configureProfile() {
    setupProfile(/* enabled= */ true, /* recordMetrics= */ false, /* callback= */ null);
}
```
