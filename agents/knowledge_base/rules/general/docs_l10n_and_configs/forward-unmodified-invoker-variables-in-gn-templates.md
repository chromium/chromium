---
name: general/forward-unmodified-invoker-variables-in-gn-templates
rubric: readability
severity: nit
---

**Rule.** In GN templates, forward unmodified invoker variables directly to
inner targets using `forward_variables_from(invoker, [ ... ])` (or wildcard
forwarding with exclusion lists) instead of copying them through intermediate
local variables. Only assign invoker parameters to intermediate variables when
they must be inspected, transformed, or conditionally modified before passing.

**Why.** `forward_variables_from()` is GN's standard mechanism for forwarding
caller arguments into inner targets. Assigning unmodified invoker variables to
intermediate local variables adds unnecessary indirection and boilerplate.

## Examples

Bad:

```cpp
template("custom_library") {
  _sources = invoker.sources

  source_set(target_name) {
    forward_variables_from(invoker, [ "defines", "include_dirs" ])
    sources = _sources
  }
}
```

Good:

```cpp
template("custom_library") {
  source_set(target_name) {
    forward_variables_from(invoker, [
                             "defines",
                             "include_dirs",
                             "sources",
                           ])
  }
}
```
