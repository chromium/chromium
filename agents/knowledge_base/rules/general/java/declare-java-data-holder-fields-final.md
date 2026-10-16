---
name: general/declare-java-data-holder-fields-final
rubric: readability
severity: optional
---

**Rule.** In Java data-holder, state-snapshot, or value classes, declare member
fields as `final` and initialize them via a constructor rather than exposing
mutable non-final fields. Avoid post-instantiation mutation by keeping data
snapshot instances immutable.

**Why.** Marking fields `final` prevents accidental mutation after
instantiation, enforces immutability for state snapshots and data carriers, and
guarantees safe publication across threads under the Java Memory Model.

## Examples

Bad:

```cpp
@NullMarked
public final class ChildProcessConnectionState {
    public boolean isWaivedBound;
    public boolean isIsNotPerceptibleBound;
    public boolean isVisibleBound;
    public boolean isStrongBound;
}
```

Good:

```cpp
@NullMarked
public final class ChildProcessConnectionState {
    public final boolean isWaivedBound;
    public final boolean isIsNotPerceptibleBound;
    public final boolean isVisibleBound;
    public final boolean isStrongBound;

    public ChildProcessConnectionState(
            boolean isWaivedBound,
            boolean isNotPerceptibleBound,
            boolean isVisibleBound,
            boolean isStrongBound) {
        this.isWaivedBound = isWaivedBound;
        this.isIsNotPerceptibleBound = isNotPerceptibleBound;
        this.isVisibleBound = isVisibleBound;
        this.isStrongBound = isStrongBound;
    }
}
```
