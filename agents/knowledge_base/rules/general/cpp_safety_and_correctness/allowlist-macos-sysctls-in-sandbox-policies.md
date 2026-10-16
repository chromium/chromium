---
name: general/allowlist-macos-sysctls-in-sandbox-policies
rubric: correctness
severity: blocking
---

**Rule.** When querying macOS system information via sysctlbyname(), sysctl(),
or helper functions (such as base::mac::IsVirtualMachine()) in code executed by
sandboxed processes, ensure the underlying sysctl name is allowlisted in the
macOS sandbox policy (sandbox/policy/mac/common.sb for shared policies or the
process-specific .sb file). Alternatively, query and cache the system property
before sandbox lockdown or broker it through the browser process.

**Why.** macOS sandboxed child processes operate under Seatbelt sandbox policies
with a default-deny stance. Querying an unpermitted sysctl directly or via
helper utilities from sandboxed code triggers a sandbox violation or process
crash.

## Examples

Bad:

```cpp
// base/cpu.cc (invoked during process initialization in sandboxed child processes)
void CPU::Initialize() {
#if BUILDFLAG(IS_MAC)
  // Queries sysctlbyname("kern.hv_vmm_present", ...) without updating sandbox policy.
  is_running_in_vm_ = mac::IsVirtualMachine();
#endif
  // ...
}
```

Good:

```cpp
;; sandbox/policy/mac/common.sb
  (sysctl-name "kern.hostname")
+ (sysctl-name "kern.hv_vmm_present")
  (sysctl-name "kern.maxfilesperproc")
```
