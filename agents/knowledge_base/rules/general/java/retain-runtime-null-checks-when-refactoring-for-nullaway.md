---
name: general/retain-runtime-null-checks-when-refactoring-for-nullaway
rubric: correctness
severity: should_fix
---

**Rule.** Do not remove or omit runtime non-null checks (such as
Objects.requireNonNull(...), assert != null, or explicit null guards) on method
parameters, incoming arguments, or container/registry setter methods when
annotating classes with @NullMarked or refactoring for NullAway. Retain
fail-fast runtime precondition checks on method parameters and incoming
arguments to guard against invalid null inputs from unannotated or permissive
callers. Additionally, in container, cache, or registry classes where get() or
lookup methods return null as an absence sentinel, enforce runtime non-null
checks (such as Objects.requireNonNull(value)) on setter or insertion methods to
prevent corrupting lookup invariants.

**Why.** NullAway static analysis in Chromium is opt-in per class, meaning
unannotated callers and external library APIs are permissive and do not enforce
non-null arguments at compile time. Removing or omitting runtime null checks
strips fail-fast protection against invalid null inputs and can lead to delayed
crashes or corrupted state. Furthermore, in containers where null indicates
absence, storing a null key or value makes lookups ambiguous between absent
entries and entries explicitly mapped to null.

## Examples

Bad:

```cpp
// Bad: Removing or omitting runtime null checks when annotating with @NullMarked
@NullMarked
public class UserDataKey<T> {
    public void attachToHost(UnownedUserDataHost host, T object) {
        // Bad: Removed Objects.requireNonNull(object) assuming @NullMarked prevents nulls
        host.set(this, object);
        mWeakHostAttachments.add(host);
    }
}

@NullMarked
public class UserDataHost {
    private final Map<Class<? extends UserData>, UserData> mUserDataMap = new HashMap<>();

    // Bad: Missing Objects.requireNonNull; unannotated callers can insert null, breaking lookup
    public <T extends UserData> T setUserData(Class<T> key, T object) {
        mUserDataMap.put(key, object);
        return object;
    }
}
```

Good:

```cpp
// Good: Retain runtime precondition checks to fail fast against unannotated callers
@NullMarked
public class UserDataKey<T> {
    public void attachToHost(UnownedUserDataHost host, T object) {
        Objects.requireNonNull(object);
        host.set(this, object);
        mWeakHostAttachments.add(host);
    }
}

@NullMarked
public class UserDataHost {
    private final Map<Class<? extends UserData>, UserData> mUserDataMap = new HashMap<>();

    public <T extends UserData> T setUserData(Class<T> key, T object) {
        Objects.requireNonNull(object);
        mUserDataMap.put(key, object);
        return object;
    }
}
```
