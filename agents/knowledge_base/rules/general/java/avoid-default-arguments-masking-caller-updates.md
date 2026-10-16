---
name: general/avoid-default-arguments-masking-caller-updates
rubric: correctness
severity: should_fix
---

**Rule.** When adding parameters to a function or defining functions with few
callers, avoid default arguments and do not retain convenience overloads that
hardcode default values solely to avoid updating existing call sites. Update all
callers and test cases explicitly so that every usage is intentionally audited
for correctness and declares clear caller intent.

**Why.** Default parameter arguments and convenience overloads allow existing
callers and tests to silently compile without supplying meaningful context,
handling new functionality, or being evaluated for the changed behavior.
Requiring explicit arguments forces all call sites to be audited during
refactorings and prevents callers from silently inheriting unintended default
state.

## Examples

Bad:

```cpp
// C++: Default arguments silently bypass auditing at call sites.
BASE_EXPORT HistogramBase* DeserializeHistogramInfo(
    base::PickleIterator* iter,
    std::string_view name_override = "");

// Java: Convenience overload retained solely to avoid updating callers.
public @Nullable ChildProcessConnection allocate(
        Context context,
        Bundle serviceBundle,
        final ChildProcessConnection.ServiceCallback serviceCallback) {
    return allocate(context, serviceBundle, serviceCallback, ChildBindingState.VISIBLE);
}
```

Good:

```cpp
// C++: Requiring explicit arguments forces call sites to be audited.
BASE_EXPORT HistogramBase* DeserializeHistogramInfo(
    base::PickleIterator* iter,
    std::string_view name_override);

// Java: Explicit parameter requires all callers and tests to declare intent.
public @Nullable ChildProcessConnection allocate(
        Context context,
        Bundle serviceBundle,
        final ChildProcessConnection.ServiceCallback serviceCallback,
        @ChildBindingState int initialBindingState) {
    assert isRunningOnLauncherThread();
    ...
}
```
