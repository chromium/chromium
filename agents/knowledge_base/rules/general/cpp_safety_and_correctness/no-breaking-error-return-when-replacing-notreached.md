---
name: general/no-breaking-error-return-when-replacing-notreached
rubric: correctness
severity: should_fix
---

**Rule.** When replacing legacy NOTREACHED() assertions in parsers or handlers
of external, OS, or third-party data formats, do not introduce breaking behavior
changes by returning an error (such as return false;). Unless all callers are
audited to handle failure gracefully, log a warning and continue with
best-effort parsing.

**Why.** Historically, NOTREACHED() was non-fatal in release builds, allowing
existing callers to succeed by silently falling through unhandled fields or
unexpected variant types. Replacing it with an early error return can break
existing callers and cause production regressions.

## Examples

Bad:

```cpp
switch (pv.get().vt) {
  case VT_LPWSTR:
    properties->set_app_id(pv.get().pwszVal);
    break;
  default:
    return false;
}
```

Good:

```cpp
switch (pv.get().vt) {
  case VT_LPWSTR:
    properties->set_app_id(pv.get().pwszVal);
    break;
  default:
    LOG(WARNING) << "Unexpected variant type: " << pv.get().vt;
    break;
}
```
