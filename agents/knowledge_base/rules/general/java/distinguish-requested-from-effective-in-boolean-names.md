---
name: general/distinguish-requested-from-effective-in-boolean-names
rubric: readability
severity: should_fix
---

**Rule.** Name boolean flags, member variables, and state fields that record
caller requests or preferences with `...Requested` (or `..._requested` /
`...SetByClient`) rather than `...Enabled` or `...Active` (or `..._enabled` /
`..._active`) when the requested state can differ from the effective runtime
state (such as when subject to priority arbitration, unbinding, or lifecycle
overrides).

**Why.** Naming a state variable `...Enabled` or `...Active` implies that the
underlying resource or subsystem is currently in that state. When multiple
preferences can be set concurrently and resolved through priority arbitration or
lifecycle conditions, naming them `...Requested` clarifies caller intent and
prevents confusion with effective runtime state.

## Examples

Bad:

```cpp
// Misleading: implies the connection or resource is currently in that active state,
// ignoring priority arbitration and lifecycle overrides.
private boolean mIsStrongBindingEnabled;
private boolean mIsVisibleBindingEnabled;
private boolean mIsNotPerceptibleBindingEnabled;
```

Good:

```cpp
// Clear: accurately distinguishes caller intent and requested state from
// the arbitrated effective system state.
private boolean mIsStrongBindingRequested;
private boolean mIsVisibleBindingRequested;
private boolean mIsNotPerceptibleBindingRequested;
```
