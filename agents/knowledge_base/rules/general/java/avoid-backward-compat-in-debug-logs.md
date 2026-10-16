---
name: general/avoid-backward-compat-in-debug-logs
rubric: design
severity: should_fix
---

**Rule.** Do not complicate debug or diagnostic log formatting to preserve
legacy formats, fixed-width column layouts, or defunct multi-slot
representations for hypothetical external parsers. Format debug output and state
strings directly around the current data model unless an explicit
machine-readable contract is required.

**Why.** Diagnostic logs and debug string representations are intended for
developer inspection and troubleshooting rather than stable machine-readable
contracts. Preserving defunct multi-slot or fixed-column legacy layouts when
underlying models change adds unnecessary branching and state logic, clutters
formatting code, and misrepresents the actual runtime state.

## Examples

Bad:

```cpp
// Emulating legacy 4-column layout for mutually exclusive states for hypothetical log compat:
boolean isWaived = mConnection.isBound() && bindingState == ChildBindingState.WAIVED;
boolean isVisible = mConnection.isBound() && bindingState == ChildBindingState.VISIBLE;
boolean isNotPerceptible =
        mConnection.isBound() && bindingState == ChildBindingState.NOT_PERCEPTIBLE;
boolean isStrong = mConnection.isBound() && bindingState == ChildBindingState.STRONG;
s.append("bindings:");
s.append(isWaived ? "W" : " ");
s.append(isVisible ? "V" : " ");
s.append(isNotPerceptible ? "N" : " ");
s.append(isStrong ? "S" : " ");
```

Good:

```cpp
// Directly formatting the active state without preserving defunct multi-column layout:
s.append("bindings:");
if (mConnection.isBound()) {
    @ChildBindingState int bindingState = getBindingState();
    switch (bindingState) {
        case ChildBindingState.WAIVED:
            s.append("W");
            break;
        case ChildBindingState.VISIBLE:
            s.append("V");
            break;
        case ChildBindingState.NOT_PERCEPTIBLE:
            s.append("N");
            break;
        case ChildBindingState.STRONG:
            s.append("S");
            break;
        default:
            break;
    }
}
```
