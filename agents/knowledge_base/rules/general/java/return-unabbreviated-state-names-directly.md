---
name: general/return-unabbreviated-state-names-directly
rubric: readability
severity: nit
---

**Rule.** When formatting a single discrete state or enum value into a debug
string (e.g., in `toString()` or state debug helpers), return the full,
unabbreviated state name directly via a switch statement returning string
literals rather than allocating a `StringBuilder` or using single-letter
abbreviations. Reserve single-letter abbreviations for composite bitmasks or
simultaneous flag sets.

**Why.** Allocating a `StringBuilder` to format a single enum or state value
introduces unnecessary heap allocation overhead and verbose boilerplate. Using
full, unabbreviated state names in debug strings ensures logs are clear and
self-explanatory without requiring developers to look up obscure single-letter
abbreviations.

## Examples

Bad:

```cpp
StringBuilder s = new StringBuilder();
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
    }
}
return s.toString();
```

Good:

```cpp
@ChildBindingState int bindingState = getBindingState();
switch (bindingState) {
    case ChildBindingState.WAIVED:
        return "WAIVED";
    case ChildBindingState.VISIBLE:
        return "VISIBLE";
    case ChildBindingState.NOT_PERCEPTIBLE:
        return "NOT_PERCEPTIBLE";
    case ChildBindingState.STRONG:
        return "STRONG";
    case ChildBindingState.UNBOUND:
        return "UNBOUND";
    default:
        return "UNKNOWN";
}
```
