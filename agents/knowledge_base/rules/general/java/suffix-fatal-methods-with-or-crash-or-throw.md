---
name: general/suffix-fatal-methods-with-or-crash-or-throw
rubric: readability
severity: optional
---

**Rule.** When a method or function synchronously throws an unchecked exception,
crashes, or terminates execution if an argument, precondition, or lookup fails,
make this fatal side effect explicit in the method name by suffixing it with
OrCrash, OrThrow, or OrDie (e.g., postCallbackToLooperOrCrash,
getColumnIndexOrThrow, ValueOrDie). Do not use a plain, benign action name that
conceals the fatal failure mode from callers.

**Why.** Benign action names (such as postCallbackToLooper or getValue)
misleadingly imply that the operation is safe or handles errors gracefully,
obscuring that invalid inputs or missing values will synchronously terminate
execution or throw an unchecked exception. Suffixing the identifier makes the
fatal contract and failure semantics transparent to callers and reviewers.

## Examples

Bad:

```cpp
// Java:
private <S extends @Nullable Object> void postCallbackToLooper(
        @Nullable Callback<S> callback, S result) {
    if (callback == null) {
        throw new UnhandledRejectionException((Throwable) result);
    }
    mHandler.post(callback.bind(result));
}

// C++:
int GetHeaderOffset(std::string_view header_name) {
    auto it = headers_.find(header_name);
    CHECK(it != headers_.end());
    return it->second;
}
```

Good:

```cpp
// Java:
private <S extends @Nullable Object> void postCallbackToLooperOrCrash(
        @Nullable Callback<S> callback, S result) {
    if (callback == null) {
        throw new UnhandledRejectionException((Throwable) result);
    }
    mHandler.post(callback.bind(result));
}

// C++:
int GetHeaderOffsetOrDie(std::string_view header_name) {
    auto it = headers_.find(header_name);
    CHECK(it != headers_.end());
    return it->second;
}
```
