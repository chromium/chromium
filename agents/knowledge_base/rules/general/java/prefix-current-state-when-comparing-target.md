---
name: general/prefix-current-state-when-comparing-target
rubric: readability
severity: should_fix
---

**Rule.** When naming variables or parameters in stateful systems, explicitly
qualify the lifecycle role rather than using a bare, unqualified name like state
or bindingState. Use a current prefix (e.g., currentBindingState, current_state)
for existing runtime state when comparing or transitioning against incoming
target or effective states, and retain an initial prefix (e.g.,
initialBindingState, initial_state) on parameters and variables that establish
the starting state for an object or process.

**Why.** Bare identifiers like state or bindingState are ambiguous because
readers cannot determine whether they represent an immutable invariant, the
current runtime state, a target state, or a starting condition. Qualifying state
identifiers with current... or initial... makes lifecycle transitions,
comparison logic, and initialization contracts explicit and self-documenting
across call chains.

## Examples

Bad:

```cpp
// Bad: unqualified existing state during comparison/transition against target state
@ChildBindingState int bindingState = getBindingState();
if (effectiveBindingState == bindingState) {
    return;
}
boolean isUpgrading = effectiveBindingState > bindingState;

// Bad: unqualified parameter representing starting lifecycle state
abstract @Nullable ChildProcessConnection doAllocate(
        Context context,
        Bundle serviceBundle,
        ChildProcessConnection.ServiceCallback serviceCallback,
        @ChildBindingState int bindingState);
```

Good:

```cpp
// Good: 'current' prefix clarifies existing state vs. incoming target state
@ChildBindingState int currentBindingState = getBindingState();
if (effectiveBindingState == currentBindingState) {
    return;
}
boolean isUpgrading = effectiveBindingState > currentBindingState;

// Good: 'initial' prefix explicitly communicates starting lifecycle state
abstract @Nullable ChildProcessConnection doAllocate(
        Context context,
        Bundle serviceBundle,
        ChildProcessConnection.ServiceCallback serviceCallback,
        @ChildBindingState int initialBindingState);
```
