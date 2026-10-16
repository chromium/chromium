---
name: general/clarify-conflicting-values-in-policy-warning-messages
rubric: testing
severity: should_fix
---

**Rule.** In enterprise policy and configuration UI string resources (.grd,
.grdp), formulate warning tooltips and localized messages to explicitly state
that policies are configured with conflicting values from multiple sources,
rather than merely stating that multiple sources exist.

**Why.** In enterprise policy tables and configuration viewers, identical values
across multiple sources operate as intended; warning indicators represent actual
value discrepancies. Describing the condition merely as 'configured from
multiple sources' is ambiguous for users and translators.

## Examples

Bad:

```cpp
<message name="IDS_UPDATER_POLICY_CONFLICT_WARNING" desc="Tooltip for a warning icon in the enterprise policy table indicating that a policy has been configured from multiple sources.">
  Policy configured from multiple sources.
</message>
```

Good:

```cpp
<message name="IDS_UPDATER_POLICY_CONFLICT_WARNING" desc="Tooltip for a warning icon in the enterprise policy table indicating that a policy has been configured differently from multiple sources.">
  Policy configured with conflicting values from multiple sources.
</message>
```
