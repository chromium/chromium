---
name: general/avoid-setgroups-zero-when-dropping-privileges-on-macos
rubric: safety
severity: blocking
---

**Rule.** When dropping process privileges on macOS, do not call setgroups(0,
nullptr) to clear supplementary group privileges. Use initgroups(username, gid)
to properly configure supplementary groups for the target unprivileged user.

**Why.** In the macOS XNU kernel, calling setgroups with ngroups == 0 resets the
supplementary group list to {wheel} (GID 0) rather than clearing it. Calling
setgroups(0, nullptr) inadvertently retains root/wheel supplementary group
privileges.

## Examples

Bad:

```cpp
// Bad: In XNU kernel, ngroups == 0 sets supplementary groups to {wheel} (GID 0).
if (setgroups(0, nullptr) != 0) {
  VPLOG(1) << "Failed to clear supplementary groups";
  Shutdown(EnterpriseCompanionStatus::FromPosixErrno(errno));
  return;
}
```

Good:

```cpp
// Good: initgroups correctly initializes supplementary groups for the target user.
if (initgroups("nobody", kNobodyGid) != 0) {
  VPLOG(1) << "Failed to initgroups";
  Shutdown(EnterpriseCompanionStatus::FromPosixErrno(errno));
  return;
}
```
