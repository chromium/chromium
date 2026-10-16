---
name: general/distinguish-presence-from-policy-predicates
rubric: readability
severity: nit
---

**Rule.** Name boolean query functions to distinguish resource presence or
availability (`has_*()`, `is_*()`) from policy or usage decisions (`use_*()`,
`should_use_*()`, `can_*()`). When a predicate combines availability with an
enable flag, configuration toggle, or policy check (e.g., `enabled && loaded`),
name it with an action- or policy-oriented prefix rather than a presence prefix.

**Why.** A method named `has_*()` or `is_*()` implies a query strictly for the
existence, loading status, or presence of a resource. Naming a combined
policy-and-presence check `has_*()` is misleading because it can return false
even when the resource is present and loaded, obscuring whether the check failed
due to configuration policy or missing resources.

## Examples

Bad:

```cpp
// Returns false even if dbghelp is loaded when the feature is disabled.
bool has_dbghelp() const {
  return g_should_use_dbghelp && has_dbghelp_;
}
```

Good:

```cpp
// Accurately reflects policy decision on whether to utilize dbghelp.
bool use_dbghelp() const {
  return g_should_use_dbghelp && has_dbghelp_;
}

// Presence check strictly inspects whether the resource is available.
bool has_dbghelp() const {
  return has_dbghelp_;
}
```
