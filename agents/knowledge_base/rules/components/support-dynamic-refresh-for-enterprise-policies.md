---
name: components/support-dynamic-refresh-for-enterprise-policies
rubric: correctness
severity: optional
---

**Rule.** When defining enterprise policies in components/policy/, set
dynamic_refresh: true under features: and handle runtime preference updates
(e.g., via PrefChangeRegistrar). Avoid policies evaluated only at browser
process startup; if startup-only evaluation is unavoidable, coordinate early
with chromeos/ash/components/policy/OWNERS for ChromeOS compatibility.

**Why.** Policies evaluated solely during browser startup do not integrate
cleanly with ChromeOS, where user profile sessions start after the initial
browser process launch. Supporting dynamic refresh allows administrators to
update settings without browser restarts and ensures proper multi-platform
support.

## Examples

Bad:

```cpp
# Policy defined without dynamic refresh or runtime observation
features:
  dynamic_refresh: false
# ...and evaluated only once during browser process startup
```

Good:

```cpp
# Support dynamic refresh so admin updates take effect at runtime
features:
  dynamic_refresh: true
# ...and observe runtime preference updates using PrefChangeRegistrar
```
