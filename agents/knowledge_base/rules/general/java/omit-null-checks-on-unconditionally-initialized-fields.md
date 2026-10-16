---
name: general/omit-null-checks-on-unconditionally-initialized-fields
rubric: readability
severity: should_fix
---

**Rule.** Do not retain @Nullable annotations, defensive if (field != null)
checks, compound guards (field != null && ...), 'assert field != null'
assertions, or assumeNonNull(field) / Objects.requireNonNull(field) wrappers on
member fields that are unconditionally initialized in constructors or field
initializers. In @NullMarked Java classes where references are non-null by
default, access unconditionally initialized fields directly without dead null
checks or assertion wrappers.

**Why.** Defensive null checks, compound boolean guards, @Nullable annotations,
'assert != null', and assumeNonNull wrappers on guaranteed non-null fields
introduce dead code, weaken static nullability analysis under NullAway, and
mislead readers into assuming the field may still be null.

## Examples

Bad:

```cpp
// Bad: Retaining @Nullable, defensive null checks, compound guards, assert != null, or assumeNonNull on unconditionally initialized fields
private @Nullable LegacyChildServiceConnection mNotPerceptibleBinding;

public void bind() {
    assert mNotPerceptibleBinding != null;
    assumeNonNull(mNotPerceptibleBinding).bindServiceConnection();
}

public void destroy() {
    if (mNotPerceptibleBinding != null) {
        mNotPerceptibleBinding.unbindServiceConnection(null);
    }
}

public boolean isBound() {
    return mNotPerceptibleBinding != null && mNotPerceptibleBinding.isBound();
}
```

Good:

```cpp
// Good: Non-null by default in @NullMarked classes; access unconditionally initialized fields directly
private LegacyChildServiceConnection mNotPerceptibleBinding;

public void bind() {
    mNotPerceptibleBinding.bindServiceConnection();
}

public void destroy() {
    mNotPerceptibleBinding.unbindServiceConnection(null);
}

public boolean isBound() {
    return mNotPerceptibleBinding.isBound();
}
```
