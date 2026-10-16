---
name: general/generalize-multistage-error-strings
rubric: testing
severity: should_fix
---

**Rule.** When defining localized error messages for multi-stage operations
(such as updating, installing, or syncing) in `.grd` and `.grdp` files, phrase
user-facing messages around the overarching operation rather than a specific
sub-stage unless the failure condition is exclusive to that stage. Ensure
message descriptions (`desc`) reflect the broader operational scope.

**Why.** Multi-stage operations often share error conditions across phases (such
as running out of disk space during download, unpacking, or installation).
Phrasing strings around the overarching operation enables clean reuse across
stages, minimizes translation overhead, and avoids displaying inaccurate or
confusing sub-stage error messages to users.

## Examples

Bad:

```cpp
<message name="IDS_UPDATER_ERROR_DISK_FULL" desc="Error message indicating that an update could not be downloaded because the disk is full.">
  The download failed because the system lacks sufficient storage space.
</message>
```

Good:

```cpp
<message name="IDS_UPDATER_ERROR_DISK_FULL" desc="Error message indicating that an update could not be downloaded or applied because the disk is full.">
  The update failed because the system lacks sufficient storage space.
</message>
```
